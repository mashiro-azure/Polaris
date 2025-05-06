#ifndef SCREENS_H
#define SCREENS_H

#include <u8g2.h>

typedef struct {
  char time[12];
  char GPSstatus[2];
  char latitude[12];
  char longitude[12];
  char date[8];
  char magHeading[4];
  unsigned char GPSaccquired; // this checks for the first successful gps fix
  unsigned char LORA_Txing;
} SensorState;

typedef enum {
  SCREEN_MAIN_COORD,
  SCREEN_MAIN_TIME,
  SCREEN_BROADCAST,
  SCREEN_MENU,
  SCREEN_TRACK,
  SCREEN_LAST
} ScreenState;

extern char uuid[5];
#define MAX_MENU_ITEMS 10
extern uint8_t menuItemCount;
typedef struct {
  char id[5]; // 4char + \0
  char latitude[12];
  char longitude[12];
  char battery[5];
} MenuItem;

void addMenuItem(const char *id, const char *latitude, const char *longitude,
                 const char *battery);
void screen_draw(u8g2_t *oled, ScreenState screen, SensorState sensors,
                 uint8_t currentMenuItem);
void navigateMenu(int direction);
extern double holdProgressRatio;
void updateProgressBar(u8g2_t *oled, double ratio);

#endif
