#ifndef SCREENS_H
#define SCREENS_H

#include <u8g2.h>

typedef struct {
  char time[12];
  char GPSstatus[2];
  char latitude[12];
  char longitude[12];
  char date[8];
  unsigned char GPSaccquired; // this checks for the first successful gps fix
  unsigned char LORA_Txing;
} SensorState;

typedef enum {
  SCREEN_MAIN_COORD,
  SCREEN_MAIN_TIME,
  SCREEN_BROADCAST,
  SCREEN_MENU,
  SCREEN_LAST
} ScreenState;

#define MENU_ITEM_COUNT 3 // Number of items in the menu
extern const char *menuItems[MENU_ITEM_COUNT];

void screen_draw(u8g2_t *oled, ScreenState screen, SensorState sensors,
                 uint8_t currentMenuItem);
void navigateMenu(int direction);

#endif
