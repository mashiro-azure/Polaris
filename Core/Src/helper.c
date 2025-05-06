#include "helper.h"
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
