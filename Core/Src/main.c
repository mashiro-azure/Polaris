/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2025 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "helper.h"
#include "screens.h"
#include "stm32f103xe.h"
#include "stm32f1xx_hal.h"
#include "stm32f1xx_hal_gpio.h"
#include "stm32f1xx_hal_tim.h"
#include "u8g2.h"
#include "u8x8.h"
#include <stdlib.h>
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define DEBOUNCE_DELAY 10 // in ms, defined by omron
#define SCREEN_TIMEOUT_MS 10000
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc3;

I2C_HandleTypeDef hi2c1;

TIM_HandleTypeDef htim1;

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
/**
    Input / Output
*/
volatile uint32_t buttonPressStart = 0;
volatile uint8_t buttonIsHeld = 0;   // set when button is pressed (held down)
volatile uint8_t buttonReleased = 0; // flag set when button is released
double holdProgressRatio;
static uint32_t lastDebounceTick = 0;
// lastDebounceTick: this is used for button debounce check in interrupt
uint16_t oledAddress = 0x3C << 1;
ScreenState currentScreen;
static uint8_t powersave = 0;
// powersave: this is a control boolean in main to check whether the screen is
// in powerSave mode set by u8g2.
static uint32_t lastActionTick = 0;
// lastActionTick: this is used to check when the button is last pressed.
SensorState sensorState = {.GPSstatus = "A", .LORA_Txing = 0};

/*
  Menus
*/
char uuid[5];
uint8_t paddleSwitchPressed = 0;
volatile uint8_t currentMenuItem =
    0; // Index of the currently selected menu item

/*
  Track
*/
double distance;
double bearing;

/*
  Buzzer
*/
volatile uint8_t beepActive = 0; // When 1, the timer ISR toggles the output
volatile uint32_t lastBeepTime = 0;
volatile uint32_t beepStartTime = 0;
volatile uint32_t periodicBeepEnabled = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_ADC3_Init(void);
static void MX_TIM1_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
uint8_t u8x8_gpio_and_delay(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int,
                            void *arg_ptr) {
  switch (msg) {
  case U8X8_MSG_DELAY_MILLI:
    HAL_Delay(arg_int);
    break;
  }
  return 1; // 1 is true
}

uint8_t u8x8_byte_i2c(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int,
                      void *arg_ptr) {
  static uint8_t buffer[32]; /* u8g2/u8x8 will never send more than 32 bytes
        between START_TRANSFER and END_TRANSFER */
  static uint8_t buf_idx;
  uint8_t *data;

  switch (msg) {
  case U8X8_MSG_BYTE_SEND:
    data = (uint8_t *)arg_ptr;
    while (arg_int > 0) {
      buffer[buf_idx++] = *data;
      data++;
      arg_int--;
    }
    break;
  case U8X8_MSG_BYTE_START_TRANSFER:
    buf_idx = 0;
    break;
  case U8X8_MSG_BYTE_END_TRANSFER:
    HAL_I2C_Master_Transmit(&hi2c1, oledAddress, buffer, buf_idx, 1000);
    break;
  default:
    return 0;
  }
  return 1;
}

void screen_idle_check(u8g2_t oled) {
  // SCREEN_TRACK sleep bypass
  if (currentScreen == SCREEN_TRACK || currentScreen == SCREEN_BROADCAST) {
    return;
  }

  uint32_t tickNow = HAL_GetTick();
  if (tickNow - lastActionTick > SCREEN_TIMEOUT_MS) {
    if (powersave == 0) {
      u8g2_SetPowerSave(&oled, 1);
      powersave = 1;
    }
  } else {
    if (powersave == 1) {
      u8g2_SetPowerSave(&oled, 0);
      powersave = 0;
    }
  }
}

void screen_update_idle_tick(void)

{
  // This function set the lastActionTick, so that when interrupt fires, it
  // can wake the screen up through screen_idle_check()
  lastActionTick = HAL_GetTick();
  // Don't set powersave here, screen_idle_check() will handle that.
}

void processGPSsentence(const char *gprmcMessage, SensorState *sensorState) {
  if (strncmp(gprmcMessage, "$GPRMC", 6) == 0) {
    char *token;
    char *messageCopy = strdup(gprmcMessage);
    token = strtok(messageCopy, ",");

    // Time
    token = strtok(NULL, ",");
    if (token != NULL) {
      strncpy(sensorState->time, token, 10);
      sensorState->time[10] = 'Z';
      sensorState->time[11] = '\0';
    }

    // Status
    token = strtok(NULL, ",");
    if (token != NULL) {
      strncpy(sensorState->GPSstatus, token, 1);
      sensorState->GPSstatus[1] = '\0';
    }

    // Latitude
    token = strtok(NULL, ",");
    if (token != NULL) {
      strncpy(sensorState->latitude, token, 10);
      sensorState->latitude[10] = '\0';
    }
    token = strtok(NULL, ",");
    if (token != NULL) {
      strncat(sensorState->latitude, token, 1);
    }

    // Longitude
    token = strtok(NULL, ",");
    if (token != NULL) {
      strncpy(sensorState->longitude, token, 11);
      sensorState->longitude[11] = '\0';
    }
    token = strtok(NULL, ",");
    if (token != NULL) {
      strncat(sensorState->longitude, token, 1);
    }
    convert_rawGPS_to_dd_format(sensorState, sensorState->latitude,
                                sensorState->longitude);

    // Skip speed and course over ground
    token = strtok(NULL, ",");
    token = strtok(NULL, ",");

    // Date
    token = strtok(NULL, ",");
    if (token != NULL) {
      strncpy(sensorState->date, token, 7);
      sensorState->date[7] = '\0';
    }

    // GPS first fix accquired with valid data
    if (sensorState->GPSaccquired == 0 &&
        strcmp(sensorState->GPSstatus, "A") == 0) {
      sensorState->GPSaccquired = 1;
    }

    free(messageCopy);
  }
}

void navigateMenu(int direction) {
  if (direction == 1) { // Down
    currentMenuItem++;
    if (currentMenuItem >= menuItemCount) {
      currentMenuItem = 0; // Wrap around
    }
  } else if (direction == -1) { // Up
    if (currentMenuItem == 0) {
      currentMenuItem = menuItemCount - 1; // Wrap around
    } else {
      currentMenuItem--;
    }
  }
}

/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void) {

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick.
   */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();
  MX_ADC3_Init();
  MX_TIM1_Init();
  /* USER CODE BEGIN 2 */

  // Display Setup
  u8g2_t oled;
  u8g2_Setup_ssd1306_i2c_128x32_univision_f(&oled, U8G2_R0, u8x8_byte_i2c,
                                            u8x8_gpio_and_delay);
  u8g2_InitDisplay(&oled);
  u8g2_SetPowerSave(&oled, 0);

  u8g2_ClearDisplay(&oled);
  u8g2_SetFont(&oled, u8g2_font_8x13_mf);

  // GPS
  char gpsBuffer[128];
  uint8_t gpsBufferIndex = 0;
  uint8_t gpsBufferCapturing = 0;
  char gpsLastSentence[128];
  uint8_t tempChar;

  // Menu
  generate_uuid(uuid, 4, get_adc_based_seed(&hadc3));
  addMenuItem("1a2b", "22.321542", "113.943357", "100%%");
  addMenuItem("1a2b", "22.321542", "113.943357", "100%%");
  addMenuItem("1a2b", "22.321542", "113.943357", "100%%");
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1) {
    uint32_t now = HAL_GetTick();
    // If the button is currently held down, check for a long press.
    if (buttonIsHeld) {
      uint32_t holdTime = now - buttonPressStart;
      if (holdTime >= 2000 && holdTime < 5000) {
        // Switch to a "hold screen" if not already there.
        if (currentScreen != SCREEN_BROADCAST) {
          currentScreen = SCREEN_BROADCAST;
        }

        // Compute progress after the 2-second delay:
        // The progress bar fills from 0% (at 2 sec) to 100% (at 5 sec).
        // So the effective duration is (holdTime - 2000) capped at 3000 ms.
        uint32_t effectiveHold = holdTime - 2000;
        if (effectiveHold > 3000) {
          effectiveHold = 3000;
        }
        holdProgressRatio = (double)effectiveHold / 3000.0;
      }
      // After a full 5-second hold, automatically return to the main
      // coordinate screen.
      else if (holdTime >= 5000) {
        currentScreen = SCREEN_MAIN_COORD;
        // Optionally, clear the held flag to avoid re-entering this branch.
        buttonIsHeld = 0;

        sensorState.LORA_Txing = 1;

        beepActive = 1;
        beepStartTime = now;
        lastBeepTime = now;
        periodicBeepEnabled = 1;
        HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
      }
    }

    // If the button was just released:
    if (buttonReleased) {
      uint32_t pressDuration = now - buttonPressStart;
      buttonReleased = 0; // Clear the flag

      // If the press duration was shorter than 2 seconds, it is a short press.
      if (pressDuration < 2000) {
        screen_update_idle_tick();

        if (powersave == 1) {
          // this skips the screen switching below, so that it
          // always shows the main screen when waking up.
          currentScreen = SCREEN_MAIN_COORD;
        }
        // handle menu click
        else if (currentScreen == SCREEN_MENU) {
          currentScreen = SCREEN_TRACK;
        }
        // handle track screen exit
        else if (currentScreen == SCREEN_TRACK) {
          currentScreen = SCREEN_MAIN_COORD;
        } else {
          switch (currentScreen) {
          case SCREEN_MAIN_COORD:
            currentScreen = SCREEN_MAIN_TIME;
            break;
          case SCREEN_MAIN_TIME:
            currentScreen = SCREEN_MAIN_COORD;
            break;
          case SCREEN_BROADCAST:
            currentScreen = SCREEN_MAIN_COORD;
            break;
          default:
            break;
          }
        }
      }
      // if release button between 2-5 seconds, should return to
      // SCREEN_MAIN_COORD
      else if (pressDuration < 5000) {
        screen_update_idle_tick(); // prevent screen going to sleep immediately
                                   // after returning from SCREEN_BROADCAST
        currentScreen = SCREEN_MAIN_COORD;
      }
    }

    // Buzzer
    // PERIODIC BEEP LOGIC (only active after a 5-second hold has enabled it)
    if (periodicBeepEnabled) {
      if (!beepActive && (now - lastBeepTime >= 5000)) {
        beepActive = 1;
        beepStartTime = now;
        lastBeepTime = now;
        HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
      }
    }

    // BEEPER STOP LOGIC: Stop a beep after it has sounded for 0.5 seconds.
    if (beepActive && (now - beepStartTime >= 500)) {
      beepActive = 0;
      HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_1);
    }
    // Buzzer

    screen_idle_check(oled);
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

    /*
        GPS
    */
    // if (HAL_UART_Receive(&huart2, &tempChar, 1, 10) == HAL_OK) {
    //   if (tempChar == '$') {
    //     gpsBufferIndex = 0;
    //     gpsBuffer[gpsBufferIndex++] = tempChar;
    //     gpsBufferCapturing = 1;
    //   } else if (gpsBufferCapturing && gpsBufferIndex < sizeof(gpsBuffer)
    //   - 1) {
    //     gpsBuffer[gpsBufferIndex++] = tempChar;

    //     if (tempChar == '\n') {
    //       gpsBuffer[gpsBufferIndex] = '\0';
    //       // Only forward if it's a GPRMC sentence
    //       if (strncmp(gpsBuffer, "$GPRMC", 6) == 0) {
    //         HAL_UART_Transmit(&huart1, (uint8_t *)gpsBuffer,
    //         gpsBufferIndex,
    //                           HAL_MAX_DELAY);
    //         strncpy(gpsLastSentence, gpsBuffer,
    //                 sizeof(gpsBuffer)); // copy to gpsLastSetence for
    //                 further
    //                                     // processing
    //         gpsLastSentence[sizeof(gpsLastSentence) - 1] = '\0';
    //         processGPSsentence(gpsLastSentence, time, status, latitude,
    //                            longitude, date);
    //       }
    //       gpsBufferIndex = 0;
    //       gpsBufferCapturing = 0;
    //     }
    //   }
    // }

    //
    // test:
    strcpy(gpsLastSentence, "$GPRMC,091626.000,A,2220.2717,N,11416.1467,E,0.32,"
                            "172.25,160418,,,A*62");
    processGPSsentence(gpsLastSentence, &sensorState);
    //

    /*
        Screen Refresh
    */
    if (powersave == 0) {
      screen_draw(&oled, currentScreen, sensorState, currentMenuItem);
    }
  }
  /* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void) {
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
   * in the RCC_OscInitTypeDef structure.
   */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
   */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK) {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV2;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK) {
    Error_Handler();
  }
}

/**
 * @brief ADC3 Initialization Function
 * @param None
 * @retval None
 */
static void MX_ADC3_Init(void) {

  /* USER CODE BEGIN ADC3_Init 0 */

  /* USER CODE END ADC3_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC3_Init 1 */

  /* USER CODE END ADC3_Init 1 */

  /** Common config
   */
  hadc3.Instance = ADC3;
  hadc3.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc3.Init.ContinuousConvMode = DISABLE;
  hadc3.Init.DiscontinuousConvMode = DISABLE;
  hadc3.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc3.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc3.Init.NbrOfConversion = 1;
  if (HAL_ADC_Init(&hadc3) != HAL_OK) {
    Error_Handler();
  }

  /** Configure Regular Channel
   */
  sConfig.Channel = ADC_CHANNEL_3;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_1CYCLE_5;
  if (HAL_ADC_ConfigChannel(&hadc3, &sConfig) != HAL_OK) {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC3_Init 2 */

  /* USER CODE END ADC3_Init 2 */
}

/**
 * @brief I2C1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_I2C1_Init(void) {

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK) {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */
}

/**
 * @brief TIM1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM1_Init(void) {

  /* USER CODE BEGIN TIM1_Init 0 */

  /* USER CODE END TIM1_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};
  TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

  /* USER CODE BEGIN TIM1_Init 1 */

  /* USER CODE END TIM1_Init 1 */
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 7;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 332;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim1) != HAL_OK) {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig) != HAL_OK) {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim1) != HAL_OK) {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK) {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 166;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
  sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_1) != HAL_OK) {
    Error_Handler();
  }
  sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
  sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
  sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
  sBreakDeadTimeConfig.DeadTime = 0;
  sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
  sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
  sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
  if (HAL_TIMEx_ConfigBreakDeadTime(&htim1, &sBreakDeadTimeConfig) != HAL_OK) {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM1_Init 2 */

  /* USER CODE END TIM1_Init 2 */
  HAL_TIM_MspPostInit(&htim1);
}

/**
 * @brief USART1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_USART1_UART_Init(void) {

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 9600;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK) {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */
}

/**
 * @brief USART2 Initialization Function
 * @param None
 * @retval None
 */
static void MX_USART2_UART_Init(void) {

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 9600;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK) {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */
}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void) {
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */
  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /*Configure GPIO pin : PB12 */
  GPIO_InitStruct.Pin = GPIO_PIN_12;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : PB13 PB14 */
  GPIO_InitStruct.Pin = GPIO_PIN_13 | GPIO_PIN_14;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */
  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
  if (GPIO_Pin == GPIO_PIN_12) {
    uint32_t current_tick = HAL_GetTick();

    if (current_tick - lastDebounceTick > DEBOUNCE_DELAY) {
      lastDebounceTick = current_tick;
      if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_12) == GPIO_PIN_RESET) {
        // Button pressed (falling edge)
        buttonPressStart = current_tick;
        buttonIsHeld = 1;
      } else {
        // Button released (rising edge)
        buttonIsHeld = 0;
        buttonReleased = 1;
      }
    }
  } else if (GPIO_Pin == GPIO_PIN_13) {
    uint32_t current_tick = HAL_GetTick();

    if (current_tick - lastDebounceTick > DEBOUNCE_DELAY) {
      lastDebounceTick = current_tick;
      if (!paddleSwitchPressed) {
        paddleSwitchPressed = 1;
        navigateMenu(1);
        currentScreen = SCREEN_MENU;
        screen_update_idle_tick();
      } else {
        paddleSwitchPressed = 0;
      }
    }

  } else if (GPIO_Pin == GPIO_PIN_14) {
    uint32_t current_tick = HAL_GetTick();

    if (current_tick - lastDebounceTick > DEBOUNCE_DELAY) {
      lastDebounceTick = current_tick;
      if (!paddleSwitchPressed) {
        paddleSwitchPressed = 1;
        navigateMenu(-1);
        currentScreen = SCREEN_MENU;
        screen_update_idle_tick();
      } else {
        paddleSwitchPressed = 0;
      }
    }
  }
}
/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void) {
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state
   */
  __disable_irq();
  while (1) {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef USE_FULL_ASSERT
/**
 * @brief  Reports the name of the source file and the source line number
 *         where the assert_param error has occurred.
 * @param  file: pointer to the source file name
 * @param  line: assert_param error line source number
 * @retval None
 */
void assert_failed(uint8_t *file, uint32_t line) {
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line
     number, ex: printf("Wrong parameters value: file %s on line %d\r\n",
     file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
