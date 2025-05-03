#ifndef HELPER_H
#define HELPER_H

#include <stddef.h>
#include <stdint.h>
void generate_uuid(char *uuid, size_t length, uint16_t seed);
uint32_t
get_adc_based_seed(ADC_HandleTypeDef *hadc); // clangd: unknown_typename, idc

#endif
