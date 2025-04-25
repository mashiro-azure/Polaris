#ifndef SCREENS_H
#define SCREENS_H

#include <u8g2.h>

typedef struct {
  char time[11];
  char GPSstatus[2];
  char latitude[12];
  char longitude[12];
  char date[8];
  unsigned char GPSaccquired; // this checks for the first successful gps fix
  unsigned char LORA_Txing;
} SensorState;

typedef enum { SCREEN_MAIN, SCREEN_INFO, SCREEN_LAST } ScreenState;

void screen_draw(u8g2_t *oled, ScreenState screen, SensorState sensors);

#endif
