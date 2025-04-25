#include "screens.h"
#include "u8g2.h"

void draw_los_box(u8g2_t *oled, u8g2_uint_t originX, u8g2_uint_t originY,
                  u8g2_uint_t lengthX, u8g2_uint_t lengthY) {
  // this function calculates the LOS box, especially the diagonal lines
  // -1 to prevent pixel overflow
  u8g2_DrawFrame(oled, originX, originY, lengthX, lengthY);
  u8g2_DrawLine(oled, originX, originY, originX + lengthX - 1,
                originY + lengthY - 1); // top left to bottom right
  u8g2_DrawLine(oled, originX + lengthX - 1, originY, originX,
                originY + lengthY - 1); // top right to bottom left
}

void draw_battery_box(u8g2_t *oled, float batteryPercentage) {
  // this function assumes a 128x32 display
  u8g2_uint_t displayWidth = u8g2_GetDisplayWidth(oled);
  u8g2_uint_t displayHeight = u8g2_GetDisplayHeight(oled);

  // positiion laterally
  u8g2_uint_t originX = displayWidth - 3;
  u8g2_uint_t lengthY = (int)((displayHeight - 4) * batteryPercentage);
  u8g2_uint_t originY = displayHeight - lengthY - 2;
  u8g2_uint_t lengthX = displayWidth - 1;

  u8g2_DrawBox(oled, originX, originY, lengthX, lengthY);
}

void screen_draw(u8g2_t oled, ScreenState screen) {
  u8g2_ClearBuffer(&oled);

  switch (screen) {
  case SCREEN_MAIN:
    u8g2_DrawStr(&oled, 0, 10, "main");
    draw_los_box(&oled, 0, 10, 32, 12);
    draw_battery_box(&oled, 1.0);
    break;

  case SCREEN_INFO:
    u8g2_DrawStr(&oled, 0, 10, "info");
    break;

  default:
    break;
  }

  u8g2_SendBuffer(&oled);
}
