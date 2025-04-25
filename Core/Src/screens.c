#include "screens.h"
#include "icons.h"
#include "u8g2.h"
#include <string.h>

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

void draw_centered_text_1Line(u8g2_t *oled, const char *text,
                              u8g2_uint_t originXoffset,
                              u8g2_uint_t originYoffset, u8g2_uint_t areaWidth,
                              u8g2_uint_t areaHeight) {
  u8g2_uint_t textWidth = u8g2_GetStrWidth(oled, text);

  // get origin for text
  u8g2_uint_t originX = originXoffset + ((areaWidth - textWidth) / 2);
  u8g2_uint_t originY =
      originYoffset + ((areaHeight / 2) + (u8g2_GetFontAscent(oled) / 2));
  u8g2_DrawStr(oled, originX, originY, text);
}

void screen_draw(u8g2_t *oled, ScreenState screen, SensorState sensors) {
  u8g2_ClearBuffer(oled);

  switch (screen) {
  case SCREEN_MAIN:
    if (strcmp(sensors.GPSstatus, "A") == 0) {
      u8g2_DrawXBM(oled, 2, 2, LOCATION_PIN_SOLID_WIDTH,
                   LOCATION_PIN_SOLID_HEIGHT, location_pin_solid_bits);
    }
    if (sensors.LORA_Txing == 1) {
      u8g2_DrawXBM(oled, 2, 18, RSS_SOLID_WIDTH, RSS_SOLID_HEIGHT,
                   rss_solid_bits);
    }

    // the two text in the middle (128 (screen width)- 4 (battery status)-
    // 12 (icons) = 56px for both)
    if (sensors.GPSaccquired == 0) {
      // GPS was never accquired, don't trust the date and time
      draw_los_box(oled, 16, 0, 56, 32);
      draw_centered_text_1Line(oled, "LOS", 72, 0, 56, 32);
    }

    draw_battery_box(oled, 1.0);

    break;

  case SCREEN_INFO:
    u8g2_DrawStr(oled, 0, 10, "info");
    break;

  default:
    break;
  }

  u8g2_SendBuffer(oled);
}
