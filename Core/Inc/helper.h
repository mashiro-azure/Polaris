#ifndef HELPER_H
#define HELPER_H

#include "lis3mdl_reg.h"
#include "screens.h"
#include "stm32f1xx_hal.h"
#include "stm32f1xx_hal_adc.h"
#include <stddef.h>
#include <stdint.h>

typedef struct {
  double distance;
  double bearing;
} DistanceBearing;

#define MAG_THRESHOLD 60.0f

void generate_uuid(char *uuid, size_t length, uint32_t seed);
uint32_t
get_adc_based_seed(ADC_HandleTypeDef *hadc); // clangd: unknown_typename, idc
void convert_rawGPS_to_dd_format(SensorState *sensorState, const char *rawLat,
                                 const char *rawLong);
DistanceBearing calculate_distance_and_bearing(SensorState sensorState,
                                               const char *targetLatitude,
                                               const char *targetLongitude);
double calculate_relative_bearing(SensorState sensorState,
                                  const double targetBearing);

float read_mag(char *buf, size_t buf_size, const stmdev_ctx_t *dev_ctx);

#endif
