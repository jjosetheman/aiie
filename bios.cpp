#include <string.h>
#include "globals.h"
#include "bios.h"

#include "applevm.h"
#include "physicalkeyboard.h"
#include "physicaldisplay.h"
#include "cpu.h"

#ifdef TEENSYDUINO
#include <Bounce2.h>
#include "teensy-paddles.h"
extern Bounce resetButtonDebouncer;
extern void runDebouncer();
#endif

// Experimenting with using EXTMEM to cache all the filenames in a directory
#ifndef TEENSYDUINO
#define EXTMEM
#endif

struct _cacheEntry {
  char fn[BIOS_MAXPATH];
};
#define BIOSCACHESIZE 1024 // hope that's enough files?
EXTMEM char cachedPath[BIOS_MAXPATH] = {0};
EXTMEM char cachedFilter[BIOS_MAXPATH] = {0};
EXTMEM struct _cacheEntry biosCache[BIOSCACHESIZE];
uint16_t numCacheEntries = 0;


enum {
  ACT_EXIT = 1,
  ACT_RESET = 2,
  ACT_COLDBOOT = 3,
  ACT_MONITOR = 4,
  ACT_DISPLAYTYPE = 5,
  ACT_DEBUG = 6,
  ACT_DISK1 = 7,
  ACT_DISK2 = 8,
  ACT_HD1 = 9,
  ACT_HD2 = 10,
  ACT_VOLPLUS = 11,
  ACT_VOLMINUS = 12,
  ACT_SUSPEND = 13,
  ACT_RESTORE = 14,
  ACT_PADX_INV = 15,
  ACT_PADY_INV = 16,
  ACT_PADDLES = 17,
  ACT_SPEED = 18,
  ACT_ABOUT = 19,
};

#define NUM_TITLES 4
const char *menuTitles[NUM_TITLES] = { "Aiie", "VM", "Hardware", "Disks" };
const uint8_t titleWidths[NUM_TITLES] = {45, 28, 80, 45 };

const uint8_t aiieActions[] = { ACT_ABOUT };

const uint8_t vmActions[] = { ACT_EXIT, ACT_RESET, ACT_COLDBOOT, ACT_MONITOR,
			      ACT_DEBUG, ACT_SUSPEND, ACT_RESTORE };
const uint8_t hardwareActions[] = { ACT_DISPLAYTYPE,  ACT_SPEED,
				    ACT_PADX_INV, ACT_PADY_INV,
				    ACT_PADDLES, ACT_VOLPLUS, ACT_VOLMINUS };
const uint8_t diskActions[] = { ACT_DISK1, ACT_DISK2, 
				ACT_HD1, ACT_HD2 };

#define CPUSPEED_HALF 0
#define CPUSPEED_FULL 1
#define CPUSPEED_DOUBLE 2
#define CPUSPEED_QUAD 3

const char *staticPathConcat(const char *rootPath, const char *filePath)
{
  static char buf[MAXPATH];
  strncpy(buf, rootPath, sizeof(buf)-1);
  strncat(buf, filePath, sizeof(buf)-strlen(buf)-1);

  return buf;
}

BIOS::BIOS()
{
  selectedMenu = 1;
  selectedMenuItem = 0;

  selectedFile = -1;
  for (int8_t i=0; i<BIOS_MAXFILES; i++) {
    // Put end terminators in place; strncpy won't copy over them
    fileDirectory[i][BIOS_MAXPATH] = '\0';
  }
}

BIOS::~BIOS()
{
}

void BIOS::DrawMenuBar()
{
  uint8_t xpos = 0;

  if (selectedMenu < 0) {
    selectedMenu = NUM_TITLES-1;
  }
  selectedMenu %= NUM_TITLES;

#define XPADDING 2

  for (int i=0; i<NUM_TITLES; i++) {
    for (int x=0; x<titleWidths[i] + 2*XPADDING; x++) {
      g_display->drawPixel(xpos+x, 0, 0xFFFF);
      g_display->drawPixel(xpos+x, 16, 0xFFFF);
    }
    for (int y=0; y<=16; y++) {
      g_display->drawPixel(xpos, y, 0xFFFF);
      g_display->drawPixel(xpos + titleWidths[i] + 2*XPADDING, y, 0xFFFF);
    }

    xpos += XPADDING;

    g_display->drawString(selectedMenu == i ? M_SELECTDISABLED : M_DISABLED,
			  xpos, 2, menuTitles[i]);
    xpos += titleWidths[i] + XPADDING;
  }
}


bool BIOS::runUntilDone()
{
  // Reset the cache
  cachedPath[0] = 0;
  numCacheEntries = 0;

  g_filemanager->getRootPath(rootPath, sizeof(rootPath));

  // FIXME: abstract these constant speeds
  currentCPUSpeedIndex = CPUSPEED_FULL;
  if (g_speed == 1023000/2)
    currentCPUSpeedIndex = CPUSPEED_HALF;
  if (g_speed == 1023000*2)
    currentCPUSpeedIndex = CPUSPEED_DOUBLE;
  if (g_speed == 1023000*4)
    currentCPUSpeedIndex = CPUSPEED_QUAD;

  int8_t prevAction = ACT_EXIT;
  while (1) {
    switch (prevAction = GetAction(prevAction)) {
    case ACT_EXIT:
      goto done;
    case ACT_COLDBOOT:
      ColdReboot();
      goto done;
    case ACT_RESET:
      WarmReset();
      goto done;
    case ACT_MONITOR:
      ((AppleVM *)g_vm)->Monitor();
      goto done;
    case ACT_DISPLAYTYPE:
      g_displayType++;
      g_displayType %= 4; // FIXME: abstract max #
      ((AppleDisplay*)g_display)->displayTypeChanged();
      break;
    case ACT_ABOUT:
      showAbout();
      break;
    case ACT_SPEED:
      currentCPUSpeedIndex++;
      currentCPUSpeedIndex %= 4;
      switch (currentCPUSpeedIndex) {
      case CPUSPEED_HALF:
	g_speed = 1023000/2;
	break;
      case CPUSPEED_DOUBLE:
	g_speed = 1023000*2;
	break;
      case CPUSPEED_QUAD:
	g_speed = 1023000*4;
	break;
      default:
	g_speed = 1023000;
	break;
      }
      break;
    case ACT_DEBUG:
      g_debugMode++;
      g_debugMode %= 9; // FIXME: abstract max #
      break;
    case ACT_DISK1:
      if (((AppleVM *)g_vm)->DiskName(0)[0] != '\0') {
	((AppleVM *)g_vm)->ejectDisk(0);
      } else {
	if (SelectDiskImage("dsk,.po,nib,woz")) {
	  ((AppleVM *)g_vm)->insertDisk(0, staticPathConcat(rootPath, fileDirectory[selectedFile]), false);
	  goto done;
	}
      }
      break;
    case ACT_DISK2:
      if (((AppleVM *)g_vm)->DiskName(1)[0] != '\0') {
	((AppleVM *)g_vm)->ejectDisk(1);
      } else {
	if (SelectDiskImage("dsk,.po,nib,woz")) {
	  ((AppleVM *)g_vm)->insertDisk(1, staticPathConcat(rootPath, fileDirectory[selectedFile]), false);
	  goto done;
	}
      }
      break;
    case ACT_HD1:
      if (((AppleVM *)g_vm)->HDName(0)[0] != '\0') {
	((AppleVM *)g_vm)->ejectHD(0);
      } else {
	if (SelectDiskImage("img")) {
	  ((AppleVM *)g_vm)->insertHD(0, staticPathConcat(rootPath, fileDirectory[selectedFile]));
	  goto done;
	}
      }
      break;
    case ACT_HD2:
      if (((AppleVM *)g_vm)->HDName(1)[0] != '\0') {
	((AppleVM *)g_vm)->ejectHD(1);
      } else {
	if (SelectDiskImage("img")) {
	  ((AppleVM *)g_vm)->insertHD(1, staticPathConcat(rootPath, fileDirectory[selectedFile]));
	  goto done;
	}
      }
      break;
    case ACT_PADX_INV:
      g_invertPaddleX = !g_invertPaddleX;
#ifdef TEENSYDUINO
      ((TeensyPaddles *)g_paddles)->setRev(g_invertPaddleX, g_invertPaddleY);
#endif
      break;
    case ACT_PADY_INV:
      g_invertPaddleY = !g_invertPaddleY;
#ifdef TEENSYDUINO
      ((TeensyPaddles *)g_paddles)->setRev(g_invertPaddleX, g_invertPaddleY);
#endif
      break;
    case ACT_PADDLES:
      ConfigurePaddles();
      break;
    case ACT_VOLPLUS:
      g_volume ++;
      if (g_volume > 15) {
	g_volume = 15;
      }
      break;
    case ACT_VOLMINUS:
      g_volume--;
      if (g_volume < 0) {
	g_volume = 0;
      }
      break;

    case ACT_SUSPEND:
      g_display->clrScr();
      g_display->drawString(M_SELECTED, 80, 100,"Suspending VM...");
      g_display->flush();
      // CPU is already suspended, so this is safe...
      ((AppleVM *)g_vm)->Suspend("suspend.vm");
      break;
    case ACT_RESTORE:
      // CPU is already suspended, so this is safe...
      g_display->clrScr();
      g_display->drawString(M_SELECTED, 80, 100,"Resuming VM...");
      g_display->flush();
      ((AppleVM *)g_vm)->Resume("suspend.vm");
      break;
    }
  }

 done:
  // Undo whatever damage we've done to the screen
  g_display->redraw();
  AiieRect r = { 0, 0, 191, 279 };
  g_display->blit(r);

  // return true if any persistent setting changed that we want to store in eeprom
  return true;
}

void BIOS::WarmReset()
{
  g_cpu->Reset();
}

void BIOS::ColdReboot()
{
  g_vm->Reset();
  g_cpu->Reset();
}

uint8_t BIOS::GetAction(int8_t selection)
{
  while (1) {
    DrawMainMenu();
    while (!g_keyboard->kbhit() 
#ifdef TEENSYDUINO
	   &&
	   (resetButtonDebouncer.read() == HIGH)
#endif
	   ) {
#ifdef TEENSYDUINO
      runDebouncer();
      delay(10);
#else
      usleep(100);
#endif
      // Wait for either a keypress or the reset button to be pressed
    }

#ifdef TEENSYDUINO
    if (resetButtonDebouncer.read() == LOW) {
      // wait until it's no longer pressed
      while (resetButtonDebouncer.read() == HIGH)
	runDebouncer();
      delay(100); // wait long enough for it to debounce
      // then return an exit code
      return ACT_EXIT;
    }
#else
    // FIXME: look for F10 or ESC & return ACT_EXIT?
#endif

    // selectedMenuItem and selectedMenu can go out of bounds here, and that's okay;
    // the current menu (and the menu bar) will re-pin it appropriately...
    switch (g_keyboard->read()) {
    case PK_DARR:
      selectedMenuItem++;
      break;
    case PK_UARR:
      selectedMenuItem--;
      break;
    case PK_RARR:
      selectedMenu++;
      break;
    case PK_LARR:
      selectedMenu--;
      break;
    case PK_RET:
      {
	int8_t activeAction = getCurrentMenuAction();
	if (activeAction > 0) {
	  return activeAction;
	}
      }
      break;
    }
  }
}

int8_t BIOS::getCurrentMenuAction()
{
  int8_t ret = -1;

  switch (selectedMenu) {
  case 0: // Aiie
    if (isActionActive(aiieActions[selectedMenuItem]))
      return aiieActions[selectedMenuItem];
    break;
  case 1: // VM
    if (isActionActive(vmActions[selectedMenuItem]))
      return vmActions[selectedMenuItem];
    break;
  case 2: // Hardware
    if (isActionActive(hardwareActions[selectedMenuItem]))
      return hardwareActions[selectedMenuItem];
    break;
  case 3: // Disks
    if (isActionActive(diskActions[selectedMenuItem]))
      return diskActions[selectedMenuItem];
    break;
  }

  return ret;
}

bool BIOS::isActionActive(int8_t action)
{
  // don't return true for disk events that aren't valid
  switch (action) {
  case ACT_EXIT:
  case ACT_RESET:
  case ACT_COLDBOOT:
  case ACT_MONITOR:
  case ACT_DISPLAYTYPE:
  case ACT_SPEED:
  case ACT_ABOUT:
  case ACT_DEBUG:
  case ACT_DISK1:
  case ACT_DISK2:
  case ACT_HD1:
  case ACT_HD2:
  case ACT_SUSPEND:
  case ACT_RESTORE:
  case ACT_PADX_INV:
  case ACT_PADY_INV:
  case ACT_PADDLES:
    return true;

  case ACT_VOLPLUS:
    return (g_volume < 15);
  case ACT_VOLMINUS:
    return (g_volume > 0);
  }

  /* NOTREACHED */
  return false;
}

void BIOS::DrawAiieMenu()
{
  if (selectedMenuItem < 0)
    selectedMenuItem = sizeof(aiieActions)-1;
  selectedMenuItem %= sizeof(aiieActions);

  char buf[40];
  for (int i=0; i<sizeof(aiieActions); i++) {
    switch (aiieActions[i]) {
    case ACT_ABOUT:
      sprintf(buf, "About...");
      break;
    }

    if (isActionActive(aiieActions[i])) {
      g_display->drawString(selectedMenuItem == i ? M_SELECTED : M_NORMAL, 10, 20 + 14 * i, buf);
    } else {
      g_display->drawString(selectedMenuItem == i ? M_SELECTDISABLED : M_DISABLED, 10, 20 + 14 * i,
			    buf);
    }
  }
}

void BIOS::DrawVMMenu()
{
  if (selectedMenuItem < 0)
    selectedMenuItem = sizeof(vmActions)-1;

  selectedMenuItem %= sizeof(vmActions);

  char buf[40];
  for (int i=0; i<sizeof(vmActions); i++) {
    switch (vmActions[i]) {
    case ACT_DEBUG:
      {
	const char *templateString = "Debug: %s";
	switch (g_debugMode) {
	case D_NONE:
	  sprintf(buf, templateString, "off");
	  break;
	case D_SHOWFPS:
	  sprintf(buf, templateString, "Show FPS");
	  break;
	case D_SHOWMEMFREE:
	  sprintf(buf, templateString, "Show mem free");
	  break;
	case D_SHOWPADDLES:
	  sprintf(buf, templateString, "Show paddles");
	  break;
	case D_SHOWPC:
	  sprintf(buf, templateString, "Show PC");
	  break;
	case D_SHOWCYCLES:
	  sprintf(buf, templateString, "Show cycles");
	  break;
	case D_SHOWBATTERY:
	  sprintf(buf, templateString, "Show battery");
	  break;
	case D_SHOWTIME:
	  sprintf(buf, templateString, "Show time");
	  break;
	case D_SHOWDSK:
	  sprintf(buf, templateString, "Show Disk");
	  break;
	}
      }
      break;
    case ACT_EXIT:
      strcpy(buf, "Resume");
      break;
    case ACT_RESET:
      strcpy(buf, "Reset");
      break;
    case ACT_COLDBOOT:
      strcpy(buf, "Cold Reboot");
      break;
    case ACT_MONITOR:
      strcpy(buf, "Drop to Monitor");
      break;
    case ACT_SUSPEND:
      strcpy(buf, "Suspend VM");
      break;
    case ACT_RESTORE:
      strcpy(buf, "Restore VM");
      break;
    }

    if (isActionActive(vmActions[i])) {
      g_display->drawString(selectedMenuItem == i ? M_SELECTED : M_NORMAL, 10, 20 + 14 * i, buf);
    } else {
      g_display->drawString(selectedMenuItem == i ? M_SELECTDISABLED : M_DISABLED, 10, 20 + 14 * i, buf);
    }
  }
}

void BIOS::DrawHardwareMenu()
{
  if (selectedMenuItem < 0)
    selectedMenuItem = sizeof(hardwareActions)-1;

  selectedMenuItem %= sizeof(hardwareActions);

  char buf[40];
  for (int i=0; i<sizeof(hardwareActions); i++) {
    switch (hardwareActions[i]) {
    case ACT_DISPLAYTYPE:
      {
	const char *templateString = "Display: %s";
	switch (g_displayType) {
	case m_blackAndWhite:
	  sprintf(buf, templateString, "B&W");
	  break;
	case m_monochrome:
	  sprintf(buf, templateString, "Mono");
	  break;
	case m_ntsclike:
	  sprintf(buf, templateString, "NTSC-like");
	  break;
	case m_perfectcolor:
	  sprintf(buf, templateString, "RGB");
	  break;
	}
      }
      break;
    case ACT_SPEED:
      {
	const char *templateString = "CPU Speed: %s";
	switch (currentCPUSpeedIndex) {
	case CPUSPEED_HALF:
	  sprintf(buf, templateString, "Half [511.5 kHz]");
	  break;
	case CPUSPEED_DOUBLE:
	  sprintf(buf, templateString, "Double (2.046 MHz)");
	  break;
	case CPUSPEED_QUAD:
	  sprintf(buf, templateString, "Quad (4.092 MHz)");
	  break;
	default:
	  sprintf(buf, templateString, "Normal (1.023 MHz)");
	  break;
	}
      }
      break;
    case ACT_PADX_INV:
      if (g_invertPaddleX)
	strcpy(buf, "Paddle X inverted");
      else
	strcpy(buf, "Paddle X normal");
      break;
    case ACT_PADY_INV:
      if (g_invertPaddleY)
	strcpy(buf, "Paddle Y inverted");
      else
	strcpy(buf, "Paddle Y normal");
      break;
    case ACT_PADDLES:
      strcpy(buf, "Configure paddles");
      break;
    case ACT_VOLPLUS:
      strcpy(buf, "Volume +");
      break;
    case ACT_VOLMINUS:
      strcpy(buf, "Volume -");
      break;
    }

    if (isActionActive(hardwareActions[i])) {
      g_display->drawString(selectedMenuItem == i ? M_SELECTED : M_NORMAL, 10, 20 + 14 * i, buf);
    } else {
      g_display->drawString(selectedMenuItem == i ? M_SELECTDISABLED : M_DISABLED, 10, 20 + 14 * i, buf);
    }
  }
  
  // draw the volume bar                                                                            
  uint16_t volCutoff = 300.0 * (float)((float) g_volume / 15.0);
  for (uint8_t y=234; y<=235; y++) {
    for (uint16_t x = 0; x< 300; x++) {
      g_display->drawPixel( x, y, x <= volCutoff ? 0xFFFF : 0x0010 );
    }
  }
}

void BIOS::DrawDisksMenu()
{
  if (selectedMenuItem < 0)
    selectedMenuItem = sizeof(diskActions)-1;

  selectedMenuItem %= sizeof(diskActions);

  char buf[80];
  for (int i=0; i<sizeof(diskActions); i++) {
    switch (diskActions[i]) {
    case ACT_DISK1:
    case ACT_DISK2:
      {
	const char *insertedDiskName = ((AppleVM *)g_vm)->DiskName(diskActions[i]==ACT_DISK2 ? 1 : 0);
	// Get the name of the file; strip off the directory
	const char *endPtr = &insertedDiskName[strlen(insertedDiskName)-1];
	while (endPtr != insertedDiskName &&
	       *endPtr != '/') {
	  endPtr--;
	}
	if (*endPtr == '/') {
	  endPtr++;
	}

	if (insertedDiskName[0]) {
	  snprintf(buf, sizeof(buf), "Eject Disk %d [%s]", diskActions[i]==ACT_DISK2 ? 2 : 1, endPtr);
	} else {
	  sprintf(buf, "Insert Disk %d", diskActions[i]==ACT_DISK2 ? 2 : 1);
	}
      }
      break;
    case ACT_HD1:
    case ACT_HD2:
      {
	const char *insertedDiskName = ((AppleVM *)g_vm)->HDName(diskActions[i]==ACT_HD2 ? 1 : 0);
	// Get the name of the file; strip off the directory
	const char *endPtr = &insertedDiskName[strlen(insertedDiskName)-1];
	while (endPtr != insertedDiskName &&
	       *endPtr != '/') {
	  endPtr--;
	}
	if (*endPtr == '/') {
	  endPtr++;
	}

	if (insertedDiskName[0]) {
	  snprintf(buf, sizeof(buf), "Remove HD %d [%s]", diskActions[i]==ACT_HD2 ? 2 : 1, endPtr);
	} else {
	  sprintf(buf, "Connect HD %d", diskActions[i]==ACT_HD2 ? 2 : 1);
	}
      }
      break;
    }

    if (isActionActive(diskActions[i])) {
      g_display->drawString(selectedMenuItem == i ? M_SELECTED : M_NORMAL, 10, 20 + 14 * i, buf);
    } else {
      g_display->drawString(selectedMenuItem == i ? M_SELECTDISABLED : M_DISABLED, 10, 20 + 14 * i, buf);
    }
  }
}


void BIOS::DrawCurrentMenu()
{
  switch (selectedMenu) {
  case 0: // Aiie
    DrawAiieMenu();
    break;
  case 1: // VM
    DrawVMMenu();
    break;
  case 2: // Hardware
    DrawHardwareMenu();
    break;
  case 3: // Disks
    DrawDisksMenu();
    break;
  }
}

void BIOS::DrawMainMenu()
{
  g_display->clrScr();
  //  g_display->drawString(M_NORMAL, 0, 0, "BIOS Configuration");

  DrawMenuBar();

  DrawCurrentMenu();

  g_display->flush();
}

void BIOS::ConfigurePaddles()
{
  while (1) {
    bool needsUpdate = true;
    uint8_t lastPaddleX = g_paddles->paddle0();
    uint8_t lastPaddleY = g_paddles->paddle1();

    if (g_paddles->paddle0() != lastPaddleX) {
      lastPaddleX = g_paddles->paddle0();
      needsUpdate = true;
    }
    if (g_paddles->paddle1() != lastPaddleY) {
      lastPaddleY = g_paddles->paddle1();
      needsUpdate = true;
    }
    
    if (needsUpdate) {
      char buf[50];
      g_display->clrScr();
      sprintf(buf, "Paddle X: %d    ", lastPaddleX);
      g_display->drawString(M_NORMAL, 0, 12, buf);
      sprintf(buf, "Paddle Y: %d    ", lastPaddleY);
      g_display->drawString(M_NORMAL, 0, 42, buf);
      g_display->drawString(M_NORMAL, 0, 92, "Exit with any key");
      g_display->flush();
  }

    if (g_keyboard->kbhit()) {
      g_keyboard->read(); // throw out keypress
      return;
    }
  }
}

// return true if the user selects an image
// sets selectedFile (index; -1 = "nope") and fileDirectory[][] (names of up to BIOS_MAXFILES files)
bool BIOS::SelectDiskImage(const char *filter)
{
  int8_t sel = 0;
  int8_t page = 0;
  uint16_t fileCount = 0;
  while (1) {
    fileCount = DrawDiskNames(page, sel, filter);

    while (!g_keyboard->kbhit())
      ;
    switch (g_keyboard->read()) {
    case PK_DARR:
      sel++;
      sel %= BIOS_MAXFILES + 2;
      break;
    case PK_UARR:
      sel--;
      if (sel < 0)
	sel = BIOS_MAXFILES + 1;
      break;
    case PK_ESC:
      return false;
    case PK_RET:
      if (sel == 0) {
	page--;
	if (page < 0) page = 0;
	//	else sel = BIOS_MAXFILES + 1;
      }
      else if (sel == BIOS_MAXFILES+1) {
	if (fileCount == BIOS_MAXFILES) { // don't let them select 'Next' if there were no files in the list or if the list isn't full
	  page++;
	  //sel = 0;
	}
      } else if (strcmp(fileDirectory[sel-1], "../") == 0) {
	// Go up a directory (strip a directory name from rootPath)
	stripDirectory();
	page = 0;
	//sel = 0;
	continue;
      } else if (fileDirectory[sel-1][strlen(fileDirectory[sel-1])-1] == '/') {
	// Descend in to the directory. FIXME: file path length?
	strcat(rootPath, fileDirectory[sel-1]);
	sel = 0;
	page = 0;
	continue;
      } else {
	selectedFile = sel - 1;
	g_display->flush();
	return true;
      }
      break;
    }
  }
  /* NOTREACHED */
}

void BIOS::stripDirectory()
{
  rootPath[strlen(rootPath)-1] = '\0'; // remove the last character

  while (rootPath[0] && rootPath[strlen(rootPath)-1] != '/') {
    rootPath[strlen(rootPath)-1] = '\0'; // remove the last character again
  }

  // We're either at the previous directory, or we've nulled out the whole thing.

  if (rootPath[0] == '\0') {
    // Never go beyond this
    strcpy(rootPath, "/");
  }
}

uint16_t BIOS::DrawDiskNames(uint8_t page, int8_t selection, const char *filter)
{
  uint16_t fileCount = GatherFilenames(page, filter);
  g_display->clrScr();
  g_display->drawString(M_NORMAL, 0, 12, "BIOS Configuration - pick disk");

  if (page == 0) {
    g_display->drawString(selection == 0 ? M_SELECTDISABLED : M_DISABLED, 10, 50, "<Prev>");
  } else {
    g_display->drawString(selection == 0 ? M_SELECTED : M_NORMAL, 10, 50, "<Prev>");
  }

  uint8_t i;
  for (i=0; i<BIOS_MAXFILES; i++) {
    if (i < fileCount) {
      g_display->drawString((i == selection-1) ? M_SELECTED : M_NORMAL, 10, 50 + 14 * (i+1), fileDirectory[i]);
    } else {
      g_display->drawString((i == selection-1) ? M_SELECTDISABLED : M_DISABLED, 10, 50+14*(i+1), "-");
    }

  }

  // FIXME: this doesn't accurately say whether or not there *are* more.
  if (fileCount < BIOS_MAXFILES) {
    g_display->drawString((i+1 == selection) ? M_SELECTDISABLED : M_DISABLED, 10, 50 + 14 * (i+1), "<Next>");
  } else {
    g_display->drawString(i+1 == selection ? M_SELECTED : M_NORMAL, 10, 50 + 14 * (i+1), "<Next>");
  }

  g_display->flush();
  return fileCount;
}

// Read a directory, cache all the entries
uint16_t BIOS::cacheAllEntries(const char *filter)
{
  // If we've already cached this directory, then just return it
  if (numCacheEntries && !strcmp(cachedPath, rootPath) && !strcmp(cachedFilter, filter))
    return numCacheEntries;

  // Otherwise flush the cache and start over
  numCacheEntries = 0;
  strcpy(cachedPath, rootPath);
  strcpy(cachedFilter, filter);
  
  // This could be a lengthy process, so...
  g_display->clrScr();
  g_display->drawString(M_SELECTED,
                        0,
                        0,
                        "Loading...");
  g_display->flush();
  
  // read all the entries we can find
  int16_t idx = 0;
  while (1) {
    struct _cacheEntry *ce = &biosCache[numCacheEntries];
    idx = g_filemanager->readDir(rootPath, filter, ce->fn, idx, BIOS_MAXPATH);
    if (idx == -1) {
      return numCacheEntries;
    }
    idx++;
    numCacheEntries++;
    if (numCacheEntries >= BIOSCACHESIZE-1) {
      return numCacheEntries;
    }
  }
  /* NOTREACHED */
}

void BIOS::swapCacheEntries(int a, int b)
{
  struct _cacheEntry tmpEntry;
  strcpy(tmpEntry.fn, biosCache[a].fn);
  strcpy(biosCache[a].fn, biosCache[b].fn);
  strcpy(biosCache[b].fn, tmpEntry.fn);
}

// Take all the entries in the cache and sort htem
void BIOS::sortCachedEntries()
{
  if (numCacheEntries <= 1)
    return;

  bool changedAnything = true;
  while (changedAnything) {
    changedAnything = false;
    for (int i=0; i<numCacheEntries-1; i++) {
      if (strcmp(biosCache[i].fn, biosCache[i+1].fn) > 0) {
	swapCacheEntries(i, i+1);
	changedAnything = true;
      }
    }
  }  
}

uint16_t BIOS::GatherFilenames(uint8_t pageOffset, const char *filter)
{
  uint8_t startNum = 10 * pageOffset;
  uint8_t count = 0; // number we're including in our listing

  uint16_t numEntriesTotal = cacheAllEntries(filter);
  sortCachedEntries();
  if (numEntriesTotal > BIOSCACHESIZE) {
    // ... umm, this is a problem. FIXME?
  }
  struct _cacheEntry *nextEntry = biosCache;
  while (startNum) {
    nextEntry++;
    startNum--;
  }

  while (1) {
    if (nextEntry->fn[0] == 0)
      return count;

    strncpy(fileDirectory[count], nextEntry->fn, BIOS_MAXPATH);
    count++;

    if (count >= BIOS_MAXFILES) {
      return count;
    }
    nextEntry++;
  }
}

void BIOS::showAbout()
{
  g_display->clrScr();

  g_display->drawString(M_SELECTED,
			0,
			0,
			"Aiie! - an Apple //e emulator");

  g_display->drawString(M_NORMAL, 
			15, 20,
			"(c) 2017-2020 Jorj Bauer");

  g_display->drawString(M_NORMAL,
			15, 38,
			"https://github.com/JorjBauer/aiie/");

  g_display->drawString(M_NORMAL,
			0,
			200,
			"Press any key");

  g_display->flush();

  while (!g_keyboard->kbhit())
    ;
  g_keyboard->read(); // throw out the keypress
}
