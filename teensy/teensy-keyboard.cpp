#include <Arduino.h>
#include "teensy-keyboard.h"
#include <Keypad.h>
#include "LRingBuffer.h"
#include <SoftwareSerial.h>

const byte ROWS = 5;
const byte COLS = 13;

char keys[ROWS][COLS] = {
  {  '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', DEL },
  {  ESC, 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']' },
  { _CTRL, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', RET },
  { LSHFT, 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', RSHFT, 0 },
  { LOCK, '`', TAB, '\\', LA, ' ', RA, LARR, RARR, DARR, UARR, 0, 0 }
};

LRingBuffer buffer(10); // 10 keys should be plenty, right?

static uint8_t shiftedNumber[] = { '<', // ,
				   '_', // -
				   '>', // .
				   '?', // /
				   ')', // 0
				   '!', // 1
				   '@', // 2
				   '#', // 3
				   '$', // 4
				   '%', // 5
				   '^', // 6
				   '&', // 7
				   '*', // 8
				   '(', // 9
				   0,   // (: is not a key)
				   ':'  // ;
};

TeensyKeyboard::TeensyKeyboard(VMKeyboard *k) : PhysicalKeyboard(k)
{
  leftShiftPressed = false;
  rightShiftPressed = false;
  ctrlPressed = false;
  capsLock = true;
  leftApplePressed = false;
  rightApplePressed = false;

  Serial4.begin(9600, SERIAL_8N1);

  numPressed = 0;
}

TeensyKeyboard::~TeensyKeyboard()
{
}

void TeensyKeyboard::pressedKey(uint8_t key)
{
  numPressed++;

  if (key & 0x80) {
    // it's a modifier key.
    switch (key) {
    case _CTRL:
      ctrlPressed = 1;
      break;
    case LSHFT:
      leftShiftPressed = 1;
      break;
    case RSHFT:
      rightShiftPressed = 1;
      break;
    case LOCK:
      capsLock = !capsLock;
      break;
    case LA:
      leftApplePressed = 1;
      break;
    case RA:
      rightApplePressed = 1;
      break;
    }
    return;
  }

  if (key == ' ' || key == DEL || key == ESC || key == RET || key == TAB) {
    buffer.addByte(key);
    return;
  }

  if (key >= 'a' &&
      key <= 'z') {
    if (ctrlPressed) {
      buffer.addByte(key - 'a' + 1);
      return;
    }
    if (leftShiftPressed || rightShiftPressed || capsLock) {
      buffer.addByte(key - 'a' + 'A');
      return;
    }
    buffer.addByte(key);
    return;
  }

  // FIXME: can we control-shift?
  if (key >= ',' && key <= ';') {
    if (leftShiftPressed || rightShiftPressed) {
      buffer.addByte(shiftedNumber[key - ',']);
      return;
    }
    buffer.addByte(key);
    return;
  }

  if (leftShiftPressed || rightShiftPressed) {
    uint8_t ret = 0;
    switch (key) {
    case '=':
      ret = '+';
      break;
    case '[':
      ret = '{';
      break;
    case ']':
      ret = '}';
      break;
    case '\\':
      ret = '|';
      break;
    case '\'':
      ret = '"';
      break;
    case '`':
      ret = '~';
      break;
    }
    if (ret) {
      buffer.addByte(ret);
      return;
    }
  }

  // Everything else falls through.
  buffer.addByte(key);
}

void TeensyKeyboard::releasedKey(uint8_t key)
{
  numPressed--;
  if (key & 0x80) {
    // it's a modifier key.
    switch (key) {
    case _CTRL:
      ctrlPressed = 0;
      break;
    case LSHFT:
      leftShiftPressed = 0;
      break;
    case RSHFT:
      rightShiftPressed = 0;
      break;
    case LA:
      leftApplePressed = 0;
      break;
    case RA:
      rightApplePressed = 0;
      break;
    }
  }
}

bool TeensyKeyboard::kbhit()
{
  // For debugging: also allow USB serial to act as a keyboard
  if (Serial.available()) {
    buffer.addByte(Serial.read());
  }

  return buffer.hasData();
}

int8_t TeensyKeyboard::read()
{
  if (buffer.hasData()) {
    return buffer.consumeByte();
  }

  return 0;
}

// This is a non-buffered interface to the physical keyboard, as used
// by the VM.
void TeensyKeyboard::maintainKeyboard()
{
  /* Check the HC05 */
  if (Serial4.available()) {
    int key = Serial4.read();
    vmkeyboard->keyDepressed(key);
    vmkeyboard->keyReleased(key);
  }

  // For debugging: also allow USB serial to act as a keyboard
  if (Serial.available()) {
    int c = Serial.read();
    vmkeyboard->keyDepressed(c);
    vmkeyboard->keyReleased(c);
  }
}
