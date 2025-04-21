#include "screens.h"
#include "u8g2.h"

void screen_draw(u8g2_t oled, ScreenState screen) {
  u8g2_ClearBuffer(&oled);

  switch (screen) {
  case SCREEN_MAIN:
    u8g2_DrawStr(&oled, 0, 10, "main");
    break;

  case SCREEN_INFO:
    u8g2_DrawStr(&oled, 0, 10, "info");
    break;

  default:
    break;
  }

  u8g2_SendBuffer(&oled);
}
