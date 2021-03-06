#ifndef __PHYSICALDISPLAY_H
#define __PHYSICALDISPLAY_H

#include <string.h> // strncpy

#include "vmdisplay.h" // FIXME: for AiieRect

class PhysicalDisplay {
 public:
  PhysicalDisplay() { overlayMessage[0] = '\0'; }
  virtual ~PhysicalDisplay() {};

  virtual void flush() = 0; // flush any pending drawings
  virtual void redraw() = 0; // total redraw, assuming nothing
  virtual void blit(AiieRect r) = 0;   // redraw just the VM display area

  virtual void drawImageOfSizeAt(const uint8_t *img, uint16_t sizex, uint8_t sizey, uint16_t wherex, uint8_t wherey) = 0;

  virtual void drawCharacter(uint8_t mode, uint16_t x, uint8_t y, char c) = 0;
  virtual void drawString(uint8_t mode, uint16_t x, uint8_t y, const char *str) = 0;
  virtual void debugMsg(const char *msg) {   strncpy(overlayMessage, msg, sizeof(overlayMessage)); }

  virtual void drawPixel(uint16_t x, uint16_t y, uint16_t color) = 0;
  virtual void drawPixel(uint16_t x, uint16_t y, uint8_t r, uint8_t g, uint8_t b) = 0;

  virtual void drawUIPixel(uint16_t x, uint16_t y, uint16_t color) = 0;

  virtual void clrScr() = 0;

  // methods to draw in to the buffer - not directly to the screen.

  // First, methods that expect *us* to pixel-double the width...
  virtual void cacheDoubleWidePixel(uint16_t x, uint16_t y, uint8_t color) = 0;
  virtual void cache2DoubleWidePixels(uint16_t x, uint16_t y, uint8_t colorA, uint8_t colorB) = 0;

  // Then the direct-pixel methods
  virtual void cachePixel(uint16_t x, uint16_t y, uint8_t color) = 0;
  
 protected:
  char overlayMessage[40];
};

#endif
