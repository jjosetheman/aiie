#include "woz-serializer.h"
#include "globals.h"

#define WOZMAGIC 0xD5

WozSerializer::WozSerializer() : Woz(0,0)
{
}

const char *WozSerializer::diskName()
{
  if (fd != -1) {
    return g_filemanager->fileName(fd);
  }
  return "";
}

bool WozSerializer::Serialize(int8_t fd)
{
  // If we're being asked to serialize, make sure we've flushed any data first
  flush();

  uint8_t buf[13] = { WOZMAGIC,
		      (trackPointer >> 24) & 0xFF,
		      (trackPointer >> 16) & 0xFF,
		      (trackPointer >>  8) & 0xFF,
		      (trackPointer      ) & 0xFF,
		      (trackBitCounter >> 24) & 0xFF,
		      (trackBitCounter >> 16) & 0xFF,
		      (trackBitCounter >>  8) & 0xFF,
		      (trackBitCounter      ) & 0xFF,
		      trackByte,
		      trackBitIdx,
		      trackLoopCounter,
		      WOZMAGIC };
  if (g_filemanager->write(fd, buf, 13) != 13)
    return false;
  
  return true;
}

bool WozSerializer::Deserialize(int8_t fd)
{
  // Before deserializing, the caller has to re-load the right disk image!
  uint8_t buf[13];
  if (g_filemanager->read(fd, buf, 13) != 13)
    return false;

  if (buf[0] != WOZMAGIC)
    return false;
  
  trackPointer = buf[1];
  trackPointer <<= 8; trackPointer |= buf[2];
  trackPointer <<= 8; trackPointer |= buf[3];
  trackPointer <<= 8; trackPointer |= buf[4];

  trackBitCounter = buf[5];
  trackBitCounter <<= 8; trackBitCounter |= buf[6];
  trackBitCounter <<= 8; trackBitCounter |= buf[7];
  trackBitCounter <<= 8; trackBitCounter |= buf[8];

  trackByte = buf[9];
  trackBitIdx = buf[10];
  trackLoopCounter = buf[11];
  if (buf[12] != WOZMAGIC)
    return false;
  
  return true;
}

