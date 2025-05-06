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
#include "lis3mdl_reg.h"
#include "stm32f1xx_hal.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define MAG_THRESHOLD 60.0f
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */
float offsetX = 0, offsetY = 0, offsetZ = 0;
float xmin = 99999, xmax = -99999;
float ymin = 99999, ymax = -99999;
float zmin = 99999, zmax = -99999;

float last_magX = 0.0f;
float last_magY = 0.0f;
float last_magZ = 0.0f;

int sample_count = 0;

// ➕ Soft iron scale factor
float scaleX = 1.0f, scaleY = 1.0f, scaleZ = 1.0f;
static float angle_smooth = 0.0f;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static int32_t platform_write(void *handle, uint8_t reg, const uint8_t *bufp,
                              uint16_t len) {
  if (HAL_I2C_Mem_Write((I2C_HandleTypeDef *)handle, LIS3MDL_I2C_ADD_H, reg,
                        I2C_MEMADD_SIZE_8BIT, (uint8_t *)bufp, len,
                        1000) == HAL_OK)
    return 0;
  else
    return -1;
}

static int32_t platform_read(void *handle, uint8_t reg, uint8_t *bufp,
                             uint16_t len) {
  if (HAL_I2C_Mem_Read((I2C_HandleTypeDef *)handle, LIS3MDL_I2C_ADD_H, reg,
                       I2C_MEMADD_SIZE_8BIT, bufp, len, 1000) == HAL_OK)
    return 0;
  else
    return -1;
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
  /* USER CODE BEGIN 2 */
  stmdev_ctx_t dev_ctx;
  dev_ctx.write_reg = platform_write;
  dev_ctx.read_reg = platform_read;
  dev_ctx.handle = &hi2c1;

  uint8_t whoamI;
  lis3mdl_device_id_get(&dev_ctx, &whoamI);
  if (whoamI != LIS3MDL_ID) {
    Error_Handler();
  }

  lis3mdl_reset_set(&dev_ctx, PROPERTY_ENABLE);
  uint8_t rst;
  do {
    lis3mdl_reset_get(&dev_ctx, &rst);
  } while (rst);

  lis3mdl_block_data_update_set(&dev_ctx, PROPERTY_ENABLE);
  lis3mdl_data_rate_set(&dev_ctx, LIS3MDL_LP_1kHz);
  lis3mdl_full_scale_set(&dev_ctx, LIS3MDL_4_GAUSS);
  lis3mdl_operating_mode_set(&dev_ctx, LIS3MDL_CONTINUOUS_MODE);

  HAL_UART_Transmit(&huart1, (uint8_t *)"UART Init Done\r\n",
                    strlen("UART Init Done\r\n"), HAL_MAX_DELAY);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1) {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    lis3mdl_status_reg_t status;
    lis3mdl_status_get(&dev_ctx, &status);
    if (status.zyxda) {
      int16_t data_raw_magnetic[3];
      float magnetic_mG[3];

      memset(data_raw_magnetic, 0x00, 3 * sizeof(int16_t));
      lis3mdl_magnetic_raw_get(&dev_ctx, data_raw_magnetic);

      magnetic_mG[0] =
          lis3mdl_from_fs4_to_gauss(data_raw_magnetic[0]) * 1000.0f;
      magnetic_mG[1] =
          lis3mdl_from_fs4_to_gauss(data_raw_magnetic[1]) * 1000.0f;
      magnetic_mG[2] =
          lis3mdl_from_fs4_to_gauss(data_raw_magnetic[2]) * 1000.0f;

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
        continue; // 唔做 angle 更新
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
        continue; // 如果磁場太弱，唔更新 angle
      }

      // 平滑 angle（EMA）
      float alpha = 0.1f; // 濾波系數，可調整
      angle_smooth = alpha * angle + (1 - alpha) * angle_smooth;

      // 更新 last_magX/Y/Z
      last_magX = magnetic_mG[0];
      last_magY = magnetic_mG[1];
      last_magZ = magnetic_mG[2];

      // 6️⃣ Print
      char tx_buffer[128];
      snprintf(tx_buffer, sizeof(tx_buffer),
               "Mag: X = %.2f, Y = %.2f, Z = %.2f, Angle: %.2f\r\n",
               magnetic_mG[0] / 1000.0f, magnetic_mG[1] / 1000.0f,
               magnetic_mG[2] / 1000.0f, angle_smooth);
      HAL_UART_Transmit(&huart1, (uint8_t *)tx_buffer, strlen(tx_buffer),
                        HAL_MAX_DELAY);
    }
    /* USER CODE END 3 */
  }
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void) {
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
   * in the RCC_OscInitTypeDef structure.
   */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
   */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK) {
    Error_Handler();
  }
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
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void) {
  /* USER CODE BEGIN MX_GPIO_Init_1 */
  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /* USER CODE BEGIN MX_GPIO_Init_2 */
  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

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
