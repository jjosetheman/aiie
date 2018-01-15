#ifndef __TEENSY_DISPLAY_H
#define __TEENSY_DISPLAY_H

#include <Arduino.h>
#include "physicaldisplay.h"

#include "SPI.h"
#define ENABLE_ILI9341_FRAMEBUFFER
#include "ILI9341_t3n.h"

#define TEENSY_DHEIGHT 240
#define TEENSY_DWIDTH 320

enum {
  M_NORMAL = 0,
  M_SELECTED = 1,
  M_DISABLED = 2,
  M_SELECTDISABLED = 3
};

#define regtype volatile uint8_t
#define regsize uint8_t

#define cbi(reg, bitmask) *reg &= ~bitmask
#define sbi(reg, bitmask) *reg |= bitmask
#define pulse_high(reg, bitmask) { sbi(reg, bitmask); cbi(reg, bitmask); }
#define pulse_low(reg, bitmask) { cbi(reg, bitmask); sbi(reg, bitmask); }

#define cport(port, data) port &= data
#define sport(port, data) port |= data

#define vswap(type, i, j) {type t = i; i = j; j = t;}

class UTFT;
class BIOS;

class TeensyDisplay : public PhysicalDisplay {
  friend class BIOS;

 public:
  TeensyDisplay();
  virtual ~TeensyDisplay();
  
  virtual void blit(AiieRect r);
  virtual void redraw();

  void clrScr();

  virtual void drawCharacter(uint8_t mode, uint16_t x, uint8_t y, char c);
  virtual void drawString(uint8_t mode, uint16_t x, uint8_t y, const char *str);

  virtual void drawImageOfSizeAt(const uint8_t *img, uint16_t sizex, uint8_t sizey, uint16_t wherex, uint8_t wherey);

 private:
  regtype                 *P_RS, *P_WR, *P_CS, *P_RST, *P_SDA, *P_SCL, *P_ALE;
  regsize                 B_RS, B_WR, B_CS, B_RST, B_SDA, B_SCL, B_ALE;
  
  uint8_t fch, fcl; // high and low foreground colors

  void _fast_fill_16(int ch, int cl, long pix);

  void setYX(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);
  void clrXY();

  void setColor(byte r, byte g, byte b);
  void setColor(uint16_t color);
  void fillRect(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);
  virtual void drawPixel(uint16_t x, uint16_t y, uint16_t color);
  virtual void drawPixel(uint16_t x, uint16_t y, uint8_t r, uint8_t g, uint8_t b);

  inline void LCD_Writ_Bus(uint8_t VH,uint8_t VL) __attribute__((always_inline));
  inline void LCD_Write_COM(uint8_t VL) __attribute__((always_inline));
  inline void LCD_Write_DATA(uint8_t VH,uint8_t VL) __attribute__((always_inline));
  inline void LCD_Write_DATA(uint8_t VL) __attribute__((always_inline));
  inline void LCD_Write_COM_DATA(uint8_t com1,uint16_t dat1) __attribute__((always_inline));

  bool needsRedraw;
  bool driveIndicator[2];
  bool driveIndicatorDirty;

  uint16_t selectedColor;

};

#endif
