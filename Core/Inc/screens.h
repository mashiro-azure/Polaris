#ifndef SCREENS_H
#define SCREENS_H

#include <u8g2.h>

typedef enum { SCREEN_MAIN, SCREEN_INFO, SCREEN_LAST } ScreenState;

void screen_draw(u8g2_t *oled, ScreenState screen);

#endif
