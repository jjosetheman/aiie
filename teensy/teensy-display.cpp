#include <ctype.h> // isgraph
#include "teensy-display.h"

#include "bios-font.h"
#include "appleui.h"

#include "globals.h"
#include "applevm.h"

#include "ILI9341_t3.h"

#define TFT_CS 10
#define TFT_DC 9
#define TFT_RST 255
#define TFT_MOSI 11
#define TFT_SCLK 13
#define TFT_MISO 12

ILI9341_t3 display = ILI9341_t3(TFT_CS, TFT_DC, TFT_RST, TFT_MOSI, TFT_SCLK, TFT_MISO);

// RGB map of each of the lowres colors
const uint16_t loresPixelColors[16] = { 0x0000, // 0 black
					0xC006, // 1 magenta
					0x0010, // 2 dark blue
					0xA1B5, // 3 purple
					0x0480, // 4 dark green
					0x6B4D, // 5 dark grey
					0x1B9F, // 6 med blue
					0x0DFD, // 7 light blue
					0x92A5, // 8 brown
					0xF8C5, // 9 orange
					0x9555, // 10 light gray
					0xFCF2, // 11 pink
					0x07E0, // 12 green
					0xFFE0, // 13 yellow
					0x87F0, // 14 aqua
					0xFFFF  // 15 white
};

TeensyDisplay::TeensyDisplay()
{
  selectedColor = 0x0000;

  display.begin();
  display.setRotation(3);

  // LCD initialization complete

  setColor(255, 255, 255);

  clrScr();

  driveIndicator[0] = driveIndicator[1] = false;
  driveIndicatorDirty = true;
}

TeensyDisplay::~TeensyDisplay()
{
}

void TeensyDisplay::redraw()
{
  g_ui->drawStaticUIElement(UIeOverlay);

  if (g_vm) {
    g_ui->drawOnOffUIElement(UIeDisk1_state, ((AppleVM *)g_vm)->DiskName(0)[0] == '\0');
    g_ui->drawOnOffUIElement(UIeDisk2_state, ((AppleVM *)g_vm)->DiskName(1)[0] == '\0');
  }
}

void TeensyDisplay::clrScr()
{
  display.fillScreen(0x0000);
}

void TeensyDisplay::setColor(byte r, byte g, byte b)
{
}

void TeensyDisplay::setColor(uint16_t color)
{
  selectedColor = color;
}

void TeensyDisplay::fillRect(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
  if (x1>x2) {
    vswap(uint16_t, x1, x2);
  }
  if (y1 > y2) {
    vswap(uint16_t, y1, y2);
  }

  for (uint16_t y=y1; y<y2; y++) {
    for (uint16_t x=x1; x<x2; x++) {
      display.drawPixel(x, y, selectedColor);
    }
  }
}

void TeensyDisplay::drawPixel(uint16_t x, uint16_t y, uint16_t color)
{
  display.drawPixel(x, y, color);
}

void TeensyDisplay::drawPixel(uint16_t x, uint16_t y, uint8_t r, uint8_t g, uint8_t b)
{
  uint16_t color16 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | ((b & 0xF8) >> 3);

  display.drawPixel(x, y, color16);
}

void TeensyDisplay::blit(AiieRect r)
{
  #define HOFFSET 18
  #define VOFFSET 13

  // send the pixel data
  uint8_t *videoBuffer = g_vm->videoBuffer; // FIXME: poking deep

  uint16_t pixel;

  display.setAddrWindow(r.left+HOFFSET, r.top+VOFFSET, 
			r.right+HOFFSET, r.bottom+VOFFSET);


  for (uint8_t y=r.top; y<=r.bottom; y++) {
    for (uint16_t x=r.left; x<=r.right; x++) {
      pixel = y * (DISPLAYRUN >> 1) + (x >> 1);
      uint8_t colorIdx;
      if (!(x & 0x01)) {
	colorIdx = videoBuffer[pixel] >> 4;
      } else {
	colorIdx = videoBuffer[pixel] & 0x0F;
      }
      display.pushColor(loresPixelColors[colorIdx]);
    }
  }

  // draw overlay, if any
  if (overlayMessage[0]) {
    display.setTextColor(ILI9341_YELLOW, ILI9341_BLACK);
    display.setTextSize(2);
    display.setCursor(1, 240 - 16 - 12);
    display.println(overlayMessage);
  }
}

void TeensyDisplay::drawCharacter(uint8_t mode, uint16_t x, uint8_t y, char c)
{
}

void TeensyDisplay::drawString(uint8_t mode, uint16_t x, uint8_t y, const char *str)
{
  int8_t xsize = 8; // width of a char in this font

  for (int8_t i=0; i<strlen(str); i++) {
    drawCharacter(mode, x, y, str[i]);
    x += xsize; // fixme: any inter-char spacing?
  }
}

void TeensyDisplay::drawImageOfSizeAt(const uint8_t *img, 
				      uint16_t sizex, uint8_t sizey, 
				      uint16_t wherex, uint8_t wherey)
{
  uint8_t r, g, b;

  for (uint8_t y=0; y<sizey; y++) {
    for (uint16_t x=0; x<sizex; x++) {
      r = pgm_read_byte(&img[(y*sizex + x)*3 + 0]);
      g = pgm_read_byte(&img[(y*sizex + x)*3 + 1]);
      b = pgm_read_byte(&img[(y*sizex + x)*3 + 2]);
      drawPixel(x+wherex, y+wherey, (((r&248)|g>>5) << 8) | ((g&28)<<3|b>>3));
    }
  }
}
