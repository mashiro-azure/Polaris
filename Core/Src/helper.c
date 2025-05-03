#include "stm32f1xx_hal.h"
#include "stm32f1xx_hal_adc.h"
#include <stddef.h>

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
