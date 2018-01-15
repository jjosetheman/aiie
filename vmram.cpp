#ifdef TEENSYDUINO
#include <Arduino.h>
#endif

#include "vmram.h"
#include <string.h>
#include "globals.h"

#ifdef TEENSYDUINO
#include "parallelsram.h"
#endif

#ifndef TEENSYDUINO
#include <assert.h>
#else
#define assert(x) { if (!(x)) {Serial.print("assertion failed at "); Serial.println(__LINE__); delay(10000);} }
//#define assert(x) { }
#endif

// Serializing token for RAM data
#define RAMMAGIC 'R'

#ifdef TEENSYDUINO
ParallelSRAM sram;
#endif

void initRamChip()
{
#ifdef TEENSYDUINO
  // 256k RAM chip; initialize memory to 0
  for (int i=0; i<256*1024; i++) {
    sram.write(i, 0x00);
  }

  Serial.println("init'd");
#endif
}

VMRam::VMRam() {memset(preallocatedRam, 0, sizeof(preallocatedRam)); }

VMRam::~VMRam() { }

void VMRam::init()
{
  initRamChip();

#if 0
  Test();
#endif

  for (uint32_t i=0; i<sizeof(preallocatedRam); i++) {
    preallocatedRam[i] = 0;
  }
}

uint8_t VMRam::readByte(uint32_t addr) 
{
#ifdef TEENSYDUINO
  if (addr >= sizeof(preallocatedRam)) {
    uint8_t rv = sram.read(addr - sizeof(preallocatedRam));
    return rv;
  }
#endif
  return preallocatedRam[addr]; 
}

void VMRam::writeByte(uint32_t addr, uint8_t value)
{ 
#ifdef TEENSYDUINO
  if (addr >= sizeof(preallocatedRam)) {
    sram.write(addr - sizeof(preallocatedRam), value);
    return;
  }
#endif

  preallocatedRam[addr] = value;
}

bool VMRam::Serialize(int8_t fd)
{
  g_filemanager->writeByte(fd, RAMMAGIC);
  uint32_t size = sizeof(preallocatedRam);
  g_filemanager->writeByte(fd, (size >> 24) & 0xFF);
  g_filemanager->writeByte(fd, (size >> 16) & 0xFF);
  g_filemanager->writeByte(fd, (size >>  8) & 0xFF);
  g_filemanager->writeByte(fd, (size      ) & 0xFF);

  for (uint32_t pos = 0; pos < sizeof(preallocatedRam); pos++) {
    g_filemanager->writeByte(fd, preallocatedRam[pos]);
  }

#ifdef TEENSYDUINO
  for (uint32_t pos = 0; pos < 262144; pos++) {
    g_filemanager->writeByte(fd, sram.read(pos));
  }
#endif

  g_filemanager->writeByte(fd, RAMMAGIC);

  return true;
}

bool VMRam::Deserialize(int8_t fd)
{
  if (g_filemanager->readByte(fd) != RAMMAGIC) {
    return false;
  }

  uint32_t size = 0;
  size = g_filemanager->readByte(fd);
  size <<= 8;
  size |= g_filemanager->readByte(fd);
  size <<= 8;
  size |= g_filemanager->readByte(fd);
  size <<= 8;
  size |= g_filemanager->readByte(fd);

  if (size != sizeof(preallocatedRam)) {
    return false;
  }

  for (uint32_t pos = 0; pos < sizeof(preallocatedRam); pos++) {
    preallocatedRam[pos] = g_filemanager->readByte(fd);
  }

#ifdef TEENSYDUINO
  for (uint32_t pos = 0; pos < 262144; pos++) {
    sram.write(pos, g_filemanager->readByte(fd));
  }
#endif

  if (g_filemanager->readByte(fd) != RAMMAGIC) {
    return false;
  }

  return true;
}

bool VMRam::Test()
{
#ifdef TEENSYDUINO
  Serial.println("Executing external SRAM test");
  // external SRAM check.
  // Use preallocatedRam[] as a buffer; fill it with random data, 
  // copy that to the SRAM, and verify (in a different order).
  uint32_t errcount = 0;
  static char buf[256];

  sram.SetPins(); // testing: does resetting the pins fix the problems?

  for (int i=0; i<sizeof(preallocatedRam); i++) {
    preallocatedRam[i] = random(0, 255);
    sram.write(i, preallocatedRam[i]);
  }
  
  for (int i=sizeof(preallocatedRam)-1; i>=0; i--) {
    uint8_t v = sram.read(i);
    if (v !=preallocatedRam[i]) {

      sprintf(buf, "Verifying failed for 0x%X == %X [expected %X]", i, v, preallocatedRam[i]);
      Serial.println(buf);
      errcount++;
      delay(1000);
    }
  }
  sprintf(buf, "  Verify complete: %d error%s",
          errcount, errcount == 1 ? "" : "s");
  Serial.println(buf);
#endif
  return true;
}
