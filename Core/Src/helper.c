#include "helper.h"
#include "lis3mdl_reg.h"
#include "math.h"
#include "screens.h"
#include "stm32f1xx_hal.h"
#include "stm32f1xx_hal_adc.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

uint32_t get_adc_based_seed(ADC_HandleTypeDef *hadc) {
  uint32_t seed = 0;
  HAL_ADC_Init(hadc);
  HAL_ADC_Start(hadc);
  if (HAL_ADC_PollForConversion(hadc, 10) == HAL_OK) {
    seed = HAL_ADC_GetValue(hadc);
  }
  HAL_ADC_Stop(hadc);
  HAL_ADC_DeInit(hadc);
  return seed;
}

void generate_uuid(char *uuid, size_t length, uint32_t adc_seed) {
  const char charset[] = "0123456789abcdefghijklmnopqrstuvwxyz";
  uint32_t seed = adc_seed ^ HAL_GetTick();
  uint32_t state = seed + HAL_GetTick();

  // LCG parameters from Numerical Recipes:
  const uint32_t multiplier = 1664525;
  const uint32_t increment = 1013904223;

  for (size_t i = 0; i < length; i++) {
    // Update the state
    state = state * multiplier + increment;
    // Use the state to choose a character from the charset
    int random_index = state % (sizeof(charset) - 1);
    uuid[i] = charset[random_index];
  }
  uuid[length] = '\0';
}

void convert_rawGPS_to_dd_format(SensorState *sensorState, const char *rawLat,
                                 const char *rawLong) {
  // Convert latitude
  int latDegrees = atoi(rawLat) / 100;                   // Get the degrees part
  double latMinutes = atof(rawLat) - (latDegrees * 100); // Get the minutes part
  double latDecimal =
      latDegrees + (latMinutes / 60.0); // Convert to decimal degrees

  // Adjust for N/S
  if (rawLat[strlen(rawLat) - 1] == 'S') {
    latDecimal = -latDecimal;
  }

  // Convert longitude
  int lonDegrees = atoi(rawLong) / 100; // Get the degrees part
  double lonMinutes =
      atof(rawLong) - (lonDegrees * 100); // Get the minutes part
  double lonDecimal =
      lonDegrees + (lonMinutes / 60.0); // Convert to decimal degrees

  // Adjust for E/W
  if (rawLong[strlen(rawLong) - 1] == 'W') {
    lonDecimal = -lonDecimal;
  }

  // Store the converted values as strings in the SensorState structure
  snprintf(sensorState->latitude, sizeof(sensorState->latitude), "%.6f",
           latDecimal);
  snprintf(sensorState->longitude, sizeof(sensorState->longitude), "%.6f",
           lonDecimal);
}

double convert_gps_string_to_double(const char *coord) {
  char direction = coord[strlen(coord) - 1]; // Get the last character (N/S/E/W)
  double value = atof(coord);                // Convert the string to a double

  // Adjust the value based on the direction
  if (direction == 'S' || direction == 'W') {
    value = -value;
  }

  return value;
}

DistanceBearing calculate_distance_and_bearing(SensorState sensorState,
                                               const char *targetLatitude,
                                               const char *targetLongitude) {
  DistanceBearing result;
  double selfLat = convert_gps_string_to_double(sensorState.latitude);
  double selfLong = convert_gps_string_to_double(sensorState.longitude);
  double targetLat = convert_gps_string_to_double(targetLatitude);
  double targetLong = convert_gps_string_to_double(targetLongitude);

  // deg to rad
  double selfLat_rad = selfLat * M_PI / 180.0;
  double selfLong_rad = selfLong * M_PI / 180.0;
  double targetLat_rad = targetLat * M_PI / 180.0;
  double targetLong_rad = targetLong * M_PI / 180.0;

  // Haversine formula
  double dlat = targetLat_rad - selfLat_rad;
  double dlong = targetLong_rad - selfLong_rad;

  double a =
      sin(dlat / 2) * sin(dlat / 2) +
      cos(selfLat_rad) * cos(targetLat_rad) * sin(dlong / 2) * sin(dlong / 2);
  double c = 2 * atan2(sqrt(a), sqrt(1 - a));
  result.distance = 6371.0 * c * 1000; // Earth distance in meters

  // bearing
  double y = sin(targetLong_rad - selfLong_rad) * cos(targetLat_rad);
  double x = cos(selfLat_rad) * sin(targetLat_rad) -
             sin(selfLat_rad) * cos(targetLat_rad) *
                 cos(targetLong_rad - selfLong_rad);
  result.bearing = atan2(y, x) * 180.0 / M_PI; // Convert to degrees

  // Normalize bearing to 0-360 degrees
  if (result.bearing < 0) {
    result.bearing += 360;
  }
  return result;
}

double calculate_relative_bearing(SensorState sensorState,
                                  const double targetBearing) {
  double relative_bearing = targetBearing - atof(sensorState.magHeading);
  return relative_bearing;
}

float read_mag(char *buf, size_t buf_size, const stmdev_ctx_t *dev_ctx) {
  float offsetX = 0, offsetY = 0, offsetZ = 0;
  float xmin = 99999, xmax = -99999;
  float ymin = 99999, ymax = -99999;
  float zmin = 99999, zmax = -99999;

  float last_magX = 0.0f;
  float last_magY = 0.0f;
  float last_magZ = 0.0f;

  int sample_count = 0;

  // Soft iron scale factor
  float scaleX = 1.0f, scaleY = 1.0f, scaleZ = 1.0f;
  static float angle_smooth = 0.0f;

  lis3mdl_status_reg_t status;
  lis3mdl_status_get(dev_ctx, &status);
  if (status.zyxda) {
    int16_t data_raw_magnetic[3];
    float magnetic_mG[3];

    memset(data_raw_magnetic, 0x00, 3 * sizeof(int16_t));
    lis3mdl_magnetic_raw_get(dev_ctx, data_raw_magnetic);

    magnetic_mG[0] = lis3mdl_from_fs4_to_gauss(data_raw_magnetic[0]) * 1000.0f;
    magnetic_mG[1] = lis3mdl_from_fs4_to_gauss(data_raw_magnetic[1]) * 1000.0f;
    magnetic_mG[2] = lis3mdl_from_fs4_to_gauss(data_raw_magnetic[2]) * 1000.0f;

    // 1️⃣ 更新 max/min
    if (magnetic_mG[0] > xmax)
      xmax = magnetic_mG[0];
    if (magnetic_mG[0] < xmin)
      xmin = magnetic_mG[0];
    if (magnetic_mG[1] > ymax)
      ymax = magnetic_mG[1];
    if (magnetic_mG[1] < ymin)
      ymin = magnetic_mG[1];
    if (magnetic_mG[2] > zmax)
      zmax = magnetic_mG[2];
    if (magnetic_mG[2] < zmin)
      zmin = magnetic_mG[2];

    sample_count++;

    // 2️⃣ 每 100 次 sample 計 offset 和 scale（Hard + Soft Iron）
    if (sample_count >= 100) {
      offsetX = (xmax + xmin) / 2.0f;
      offsetY = (ymax + ymin) / 2.0f;
      offsetZ = (zmax + zmin) / 2.0f;

      float x_range = xmax - xmin;
      float y_range = ymax - ymin;
      float z_range = zmax - zmin;

      // 平均半徑
      float avg_range = (x_range + y_range + z_range) / 3.0f;

      scaleX = (x_range > 0) ? (x_range / avg_range) : 1.0f;
      scaleY = (y_range > 0) ? (y_range / avg_range) : 1.0f;
      scaleZ = (z_range > 0) ? (z_range / avg_range) : 1.0f;

      // 重設
      xmax = -99999;
      xmin = 99999;
      ymax = -99999;
      ymin = 99999;
      zmax = -99999;
      zmin = 99999;
      sample_count = 0;
    }

    // 3️⃣ 減 offset（Hard Iron）
    magnetic_mG[0] -= offsetX;
    magnetic_mG[1] -= offsetY;
    magnetic_mG[2] -= offsetZ;

    // 4️⃣ 除 scale（Soft Iron）
    magnetic_mG[0] /= scaleX;
    magnetic_mG[1] /= scaleY;
    magnetic_mG[2] /= scaleZ;

    // 5️⃣ 計算磁場變化 magnitude（√(Δx^2 + Δy^2 + Δz^2)）
    float deltaX = magnetic_mG[0] - last_magX;
    float deltaY = magnetic_mG[1] - last_magY;
    float deltaZ = magnetic_mG[2] - last_magZ;

    float delta_magnitude =
        sqrtf(deltaX * deltaX + deltaY * deltaY + deltaZ * deltaZ);

    // 如果變化細過 threshold，skip angle 計算
    if (delta_magnitude < MAG_THRESHOLD) {
      return angle_smooth; // 唔做 angle 更新
    }

    // 5️⃣ 計 angle
    // 把 X/Y 顛倒，或者根據 LIS3MDL 放置方式調整
    float angle = atan2f(-magnetic_mG[0], magnetic_mG[1]) * 180.0f / M_PI;
    angle += 3.3f; // 你的 declination 修正
    if (angle < 0.0f)
      angle += 360.0f;

    // 計算 XY 強度
    float xy_strength = sqrtf(magnetic_mG[0] * magnetic_mG[0] +
                              magnetic_mG[1] * magnetic_mG[1]);
    if (xy_strength < 200.0f) {
      return angle_smooth; // 如果磁場太弱，唔更新 angle
    }

    // 平滑 angle（EMA）
    float alpha = 0.1f; // 濾波系數，可調整
    angle_smooth = alpha * angle + (1 - alpha) * angle_smooth;

    // 更新 last_magX/Y/Z
    last_magX = magnetic_mG[0];
    last_magY = magnetic_mG[1];
    last_magZ = magnetic_mG[2];

    // 6️⃣ Print
    snprintf(buf, buf_size,
             "Mag: X = %.2f, Y = %.2f, Z = %.2f, Angle: %.2f\r\n",
             magnetic_mG[0] / 1000.0f, magnetic_mG[1] / 1000.0f,
             magnetic_mG[2] / 1000.0f, angle_smooth);
    return angle_smooth;
    // HAL_UART_Transmit(&huart1, (uint8_t *)tx_buffer, strlen(tx_buffer),
    //                   HAL_MAX_DELAY);
  }
  return 0.0f;
}

void processLoRaPacket(uint8_t *loraRXBuffer, uint8_t packetLength) {
  char loraRXstring[42];
  if (packetLength >= sizeof(loraRXstring))
    packetLength = sizeof(loraRXstring) - 1;

  memcpy(loraRXstring, loraRXBuffer, packetLength);
  loraRXstring[packetLength] = '\0';

  char header[6];
  MenuItem item;
  unsigned char idExist = 0;

  // Parse the contiguous comma-separated string.
  // %5[^,] reads at most 5 characters for 'header',
  // %4[^,] reads at most 4 characters for 'id',
  // %11[^,] reads at most 11 for latitude and longitude,
  // %4s reads up to 4 characters for battery.
  int ret = sscanf(loraRXstring, "%5[^,],%4[^,],%11[^,],%11[^,],%4s", header,
                   item.id, item.latitude, item.longitude, item.battery);

  if (ret == 5 && strncmp(header, ">PLRS", 5) == 0) {
    for (int i = 0; i < menuItemCount; i++) {
      if (strcmp(menuItems[i].id, item.id) == 0) {
        idExist = 1;
        strncpy(menuItems[i].latitude, item.latitude,
                sizeof(menuItems[i].latitude) - 1);
        menuItems[i].latitude[sizeof(menuItems[i].latitude) - 1] = '\0';

        strncpy(menuItems[i].longitude, item.longitude,
                sizeof(menuItems[i].longitude) - 1);
        menuItems[i].longitude[sizeof(menuItems[i].longitude) - 1] = '\0';

        strncpy(menuItems[i].battery, item.battery,
                sizeof(menuItems[i].battery) - 1);
        menuItems[i].battery[sizeof(menuItems[i].battery) - 1] = '\0';
        break;
      }
    }
    if (idExist == 0) {
      addMenuItem(item.id, item.latitude, item.longitude, item.battery);
    }
  }
}
