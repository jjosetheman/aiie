#ifndef __SDL_PRINTER_H
#define __SDL_PRINTER_H

#include <stdlib.h>

#include <SDL.h>
#include <SDL_mutex.h>
#include <SDL_events.h>

#include "physicalprinter.h"

#define HEIGHT 800
#define NATIVEWIDTH 960 // FIXME: printer can change density...                                                                                                            


//#define WIDTH 384 // emulating the teeny printer I've got                                                                                                                
#define WIDTH 960


class SDLPrinter : public PhysicalPrinter {
 public:
  SDLPrinter();
  virtual ~SDLPrinter();

  virtual void addLine(uint8_t *rowOfBits); // must be 960 pixels wide (120 bytes)

  virtual void update();

  virtual void moveDownPixels(uint8_t p);

  void savePageAsBitmap(uint32_t pageno);

 private:
  bool isDirty;
  uint16_t ypos;

  SDL_Window *window;
  SDL_Renderer *renderer;
  volatile uint8_t _hackyBitmap[WIDTH * HEIGHT];

  SDL_mutex *printerMutex;
  uint32_t currentPageNumber;
};

#endif
