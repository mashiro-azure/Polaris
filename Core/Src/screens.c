#include "screens.h"
#include "helper.h"
#include "icons.h"
#include "math.h"
#include "u8g2.h"
#include <stdio.h>
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

void draw_centered_text_2Line(u8g2_t *oled, const char *text1,
                              const char *text2, u8g2_uint_t originXoffset,
                              u8g2_uint_t originYoffset, u8g2_uint_t areaWidth,
                              u8g2_uint_t areaHeight) {
  u8g2_uint_t text1Width = u8g2_GetStrWidth(oled, text1);
  u8g2_uint_t text2Width = u8g2_GetStrWidth(oled, text2);

  u8g2_uint_t maxTextWidth;
  if (text1Width > text2Width) {
    maxTextWidth = text1Width;
  } else {
    maxTextWidth = text2Width;
  }

  u8g2_uint_t ascent = u8g2_GetAscent(oled);
  u8g2_int_t descent = u8g2_GetDescent(oled);
  u8g2_uint_t totalAdjust = (ascent - descent) * 2;

  u8g2_uint_t originX = originXoffset + ((areaWidth - maxTextWidth) / 2);
  u8g2_uint_t originY = originYoffset + ((areaHeight - totalAdjust) / 2);

  u8g2_DrawStr(oled, originX, originY + ascent, text1);
  u8g2_DrawStr(oled, originX, originY + totalAdjust - descent, text2);
}

MenuItem menuItems[MAX_MENU_ITEMS];
uint8_t menuItemCount = 0;
void displayMenu(u8g2_t *oled, uint8_t currentMenuItem) {
  u8g2_ClearDisplay(oled);
  u8g2_SetFont(oled, u8g2_font_8x13_mf);

  for (uint8_t i = 0; i < menuItemCount; i++) {
    if (i == currentMenuItem) {
      u8g2_DrawStr(oled, 0, (i + 1) * 13, "> "); // Highlight selected item
    }
    u8g2_DrawStr(oled, 10, (i + 1) * 13, menuItems[i].id);
  }
  u8g2_SendBuffer(oled);
}

void addMenuItem(const char *id, const char *latitude, const char *longitude,
                 const char *battery) {
  if (menuItemCount < MAX_MENU_ITEMS) {
    strncpy(menuItems[menuItemCount].id, id,
            sizeof(menuItems[menuItemCount].id) - 1);
    menuItems[menuItemCount].id[sizeof(menuItems[menuItemCount].id) - 1] =
        '\0'; // Null-terminate

    strncpy(menuItems[menuItemCount].latitude, latitude,
            sizeof(menuItems[menuItemCount].latitude) - 1);
    menuItems[menuItemCount]
        .latitude[sizeof(menuItems[menuItemCount].latitude) - 1] =
        '\0'; // Null-terminate

    strncpy(menuItems[menuItemCount].longitude, longitude,
            sizeof(menuItems[menuItemCount].longitude) - 1);
    menuItems[menuItemCount]
        .longitude[sizeof(menuItems[menuItemCount].longitude) - 1] =
        '\0'; // Null-terminate

    strncpy(menuItems[menuItemCount].battery, battery,
            sizeof(menuItems[menuItemCount].battery) - 1);
    menuItems[menuItemCount]
        .battery[sizeof(menuItems[menuItemCount].battery) - 1] =
        '\0'; // Null-terminate
    menuItemCount++;
  }
}

void updateProgressBar(u8g2_t *oled, double ratio) {
  // Example:
  // Let’s say you want a progress bar 50 pixels wide, starting at position (20,
  // 40).
  int barX = 0;
  int barY = u8g2_GetDisplayHeight(oled) - 5;
  int barWidth = u8g2_GetDisplayWidth(oled);
  int barHeight = 4;

  // Draw an empty rectangle for the progress bar.
  u8g2_DrawFrame(oled, barX, barY, barWidth, barHeight);

  // Compute the filled width.
  int fillWidth = (int)(barWidth * ratio);

  // Draw a filled rectangle inside the empty frame.
  u8g2_DrawBox(oled, barX, barY, fillWidth, barHeight);
}

void draw_heading_arrow(u8g2_t *oled, int center_x, int center_y,
                        double bearing) {
  // Configuration parameters (in pixels)
  const int shaft_length = 18; // Total length of the arrow's shaft.
  const int tip_base_offset =
      4; // How far back along the shaft from the tip to compute the V's base.
  const int tip_half_width = 3; // Half the width of the V tip.

  // Convert the navigation bearing to a "drawing" angle.
  // In our drawing coordinate system, 0° points to the right.
  // Since navigation bearings use 0° = north, subtract 90°:
  double draw_angle = (bearing - 90.0) * M_PI / 180.0;

  // Compute the unit vector for the arrow's orientation.
  double vx = cos(draw_angle);
  double vy = sin(draw_angle);

  // Calculate the base and tip of the shaft such that their midpoint is
  // (center_x, center_y). Shaft midpoint = (base + tip) / 2 equals center.
  int base_x = center_x - (int)((shaft_length / 2.0) * vx);
  int base_y = center_y - (int)((shaft_length / 2.0) * vy);
  int tip_x = center_x + (int)((shaft_length / 2.0) * vx);
  int tip_y = center_y + (int)((shaft_length / 2.0) * vy);

  // Draw the shaft from the base to the tip.
  u8g2_DrawLine(oled, base_x, base_y, tip_x, tip_y);

  // For the V-shaped tip: step backward from the tip along the shaft by
  // tip_base_offset.
  double vbase_x = tip_x - tip_base_offset * vx;
  double vbase_y = tip_y - tip_base_offset * vy;

  // The perpendicular to (vx, vy) is (-vy, vx) (in 2D).
  double perp_x = -vy;
  double perp_y = vx;

  // Compute the endpoints for the two arrowhead lines (forming a V).
  int left_x = (int)(vbase_x + tip_half_width * perp_x);
  int left_y = (int)(vbase_y + tip_half_width * perp_y);
  int right_x = (int)(vbase_x - tip_half_width * perp_x);
  int right_y = (int)(vbase_y - tip_half_width * perp_y);

  // Draw the V-shaped arrow tip lines from the tip of the shaft.
  u8g2_DrawLine(oled, tip_x, tip_y, left_x, left_y);
  u8g2_DrawLine(oled, tip_x, tip_y, right_x, right_y);
}

void screen_draw(u8g2_t *oled, ScreenState screen, SensorState sensors,
                 uint8_t currentMenuItem) {
  u8g2_ClearBuffer(oled);

  switch (screen) {
  case SCREEN_MAIN_COORD:
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
    } else {
      draw_centered_text_2Line(oled, sensors.latitude, sensors.longitude, 16, 0,
                               112, 32);
      // draw_centered_text_2Line(oled, sensors.date, sensors.time, 72, 0, 56,
      // 32);
    }

    draw_battery_box(oled, 1.0);
    break;

  case SCREEN_MAIN_TIME:
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
      draw_centered_text_1Line(oled, "NOTIME", 72, 0, 56, 32);
    } else {
      draw_centered_text_2Line(oled, sensors.date, sensors.time, 16, 0, 112,
                               32);
    }
    draw_battery_box(oled, 1.0);
    break;

  case SCREEN_BROADCAST:
    u8g2_SetFont(oled, u8g2_font_6x13_mf);
    draw_centered_text_2Line(oled, "Hold to", "broadcast location", 0, 0, 128,
                             27);
    updateProgressBar(oled, holdProgressRatio);
    u8g2_SetFont(oled, u8g2_font_8x13_mf);
    break;

  case SCREEN_MENU:
    u8g2_DrawStr(oled,
                 (u8g2_GetDisplayWidth(oled) - u8g2_GetStrWidth(oled, uuid)),
                 u8g2_GetFontAscent(oled), uuid);

    // Define the vertical center of the display.
    // For a 64-pixel high display, this might be 32.
    uint8_t centerY = u8g2_GetDisplayHeight(oled) / 2;
    uint8_t itemHeight = 13; // Height for each menu item

    for (uint8_t i = 0; i < menuItemCount; i++) {
      // Calculate position relative to the selected item.
      // When i equals currentMenuItem, (i - currentMenuItem) is 0 and yPos
      // equals centerY.
      int16_t yPos = centerY + (int16_t)(i - currentMenuItem) * itemHeight;

      // Draw the highlight indicator for the selected item.
      if (i == currentMenuItem) {
        u8g2_DrawStr(oled, 0, yPos, "> "); // Highlight selected item
      }

      // Draw the menu item text.
      u8g2_DrawStr(oled, 10, yPos, menuItems[i].id);
    }
    break;
    break;

  case SCREEN_TRACK:
    MenuItem selectedItem = menuItems[currentMenuItem];
    DistanceBearing db = calculate_distance_and_bearing(
        sensors, selectedItem.latitude, selectedItem.longitude);

    char buf[16]; // should be plenty for lora range, XXXXXm / 100%\0
    sprintf(buf, "%.0fm/%s", db.distance, selectedItem.battery);
    u8g2_uint_t fontHeight =
        u8g2_GetFontAscent(oled) - u8g2_GetFontDescent(oled);

    u8g2_DrawStr(oled, 0, fontHeight, selectedItem.id);
    u8g2_DrawStr(oled, 0,
                 u8g2_GetDisplayHeight(oled) + u8g2_GetFontDescent(oled), buf);
    u8g2_DrawLine(oled, 0, 14, u8g2_GetStrWidth(oled, selectedItem.id), 14);
    draw_heading_arrow(oled, u8g2_GetDisplayWidth(oled) - 24,
                       u8g2_GetDisplayHeight(oled) / 2, db.bearing);
    draw_battery_box(oled, 1.0);
    break;

  default:
    break;
  }

  u8g2_SendBuffer(oled);
}
