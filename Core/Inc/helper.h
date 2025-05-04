#ifndef HELPER_H
#define HELPER_H

#include "screens.h"
#include <stddef.h>
#include <stdint.h>
void generate_uuid(char *uuid, size_t length, uint16_t seed);
uint32_t
get_adc_based_seed(ADC_HandleTypeDef *hadc); // clangd: unknown_typename, idc
void convert_rawGPS_to_dd_format(SensorState *sensorState, const char *rawLat,
                                 const char *rawLong);

#endif
