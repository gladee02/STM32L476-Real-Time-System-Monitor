/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
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
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

typedef struct
{
    float temperature_c;
    uint32_t vdda_mv;

} monitor_measurement_t;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define MONITOR_ADC_DONE_FLAG   (1U << 0)
#define MONITOR_ADC_ERROR_FLAG  (1U << 1)

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc1;

UART_HandleTypeDef huart2;

/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* USER CODE BEGIN PV */

/* ADC measurement buffer:
   [0] = Temperature sensor
   [1] = VREFINT
*/
uint16_t adc_buffer[2] = {0};

/* Queue for transferring measurements. */
osMessageQueueId_t measurementQueueHandle;

/* Mutex to protect USART2 transmission. */
osMutexId_t uartMutexHandle;

/* Handle for the telemetry task. */
osThreadId_t telemetryTaskHandle;

/* Telemetry task configuration. */
const osThreadAttr_t telemetryTask_attributes =
{
    .name = "telemetryTask",
    .stack_size = 1024,
    .priority = (osPriority_t)osPriorityBelowNormal
};

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_ADC1_Init(void);
void StartDefaultTask(void *argument);

/* USER CODE BEGIN PFP */

HAL_StatusTypeDef Monitor_ADC_Init(void);
HAL_StatusTypeDef Monitor_ADC_Start(void);

void Monitor_ADC_Convert(float *temperature,
                         uint32_t *vdda_mv);

static void StartTelemetryTask(void *argument);

static void Monitor_UART_Send(const char *message);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_USART2_UART_Init();
  MX_ADC1_Init();
  /* USER CODE BEGIN 2 */

  /* Calibrate ADC1 before starting the scheduler. */
  if (Monitor_ADC_Init() != HAL_OK)
  {
      Error_Handler();
  }

  /* Give internal analog circuitry time to settle. */
  HAL_Delay(10);
  /* USER CODE END 2 */

  /* Init scheduler */
  osKernelInitialize();

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */

  uartMutexHandle = osMutexNew(NULL);

  if (uartMutexHandle == NULL)
  {
      Error_Handler();
  }

  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */

  measurementQueueHandle = osMessageQueueNew(
      8,
      sizeof(monitor_measurement_t),
      NULL
  );

  if (measurementQueueHandle == NULL)
  {
      Error_Handler();
  }

  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */

  telemetryTaskHandle = osThreadNew(
      StartTelemetryTask,
      NULL,
      &telemetryTask_attributes
  );

  if (telemetryTaskHandle == NULL)
  {
      Error_Handler();
  }

  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_MultiModeTypeDef multimode = {0};
  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Common config
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.ScanConvMode = ADC_SCAN_ENABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc1.Init.LowPowerAutoWait = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.NbrOfConversion = 2;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.Overrun = ADC_OVR_DATA_PRESERVED;
  hadc1.Init.OversamplingMode = DISABLE;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure the ADC multi-mode
  */
  multimode.Mode = ADC_MODE_INDEPENDENT;
  if (HAL_ADCEx_MultiModeConfigChannel(&hadc1, &multimode) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_TEMPSENSOR;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_640CYCLES_5;
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset = 0;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_VREFINT;
  sConfig.Rank = ADC_REGULAR_RANK_2;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Channel1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LD2_Pin */
  GPIO_InitStruct.Pin = LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LD2_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* Initialize and calibrate ADC1 once. */
HAL_StatusTypeDef Monitor_ADC_Init(void)
{
    return HAL_ADCEx_Calibration_Start(
        &hadc1,
        ADC_SINGLE_ENDED
    );
}


/* Start one ADC sequence using DMA. */
HAL_StatusTypeDef Monitor_ADC_Start(void)
{
    return HAL_ADC_Start_DMA(
        &hadc1,
        (uint32_t *)adc_buffer,
        2
    );
}


/* Called when the DMA transfer is complete. */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC1)
    {
    	if (defaultTaskHandle != NULL)
		{
			osThreadFlagsSet(
				defaultTaskHandle,
				MONITOR_ADC_DONE_FLAG
			);
		}
    }
}


/* Called if an ADC error occurs. */
void HAL_ADC_ErrorCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC1)
    {
    	if (defaultTaskHandle != NULL)
		{
			osThreadFlagsSet(
				defaultTaskHandle,
				MONITOR_ADC_ERROR_FLAG
			);
		}
    }
}

/* Convert raw ADC readings into engineering units. */
void Monitor_ADC_Convert(float *temperature,
                         uint32_t *vdda_mv)
{
    uint32_t raw_temp = adc_buffer[0];
    uint32_t raw_vref = adc_buffer[1];

    if (raw_vref == 0U)
    {
        *temperature = 0.0f;
        *vdda_mv = 0U;
        return;
    }

    /* Estimate analog supply voltage in millivolts. */
    *vdda_mv =
        __HAL_ADC_CALC_VREFANALOG_VOLTAGE(
            raw_vref,
            ADC_RESOLUTION_12B
        );

    /* Calculate internal die temperature in Celsius. */
    *temperature =
        (float)__HAL_ADC_CALC_TEMPERATURE(
            *vdda_mv,
            raw_temp,
            ADC_RESOLUTION_12B
        );
}

static void Monitor_UART_Send(const char *message)
{
    if (message == NULL)
    {
        return;
    }

    /* Wait until UART is available. */
    if (osMutexAcquire(
            uartMutexHandle,
            osWaitForever) == osOK)
    {
        /* Transmit the message. */
        HAL_UART_Transmit(
            &huart2,
            (uint8_t *)message,
            (uint16_t)strlen(message),
            200
        );

        /* Release UART for other tasks. */
        osMutexRelease(uartMutexHandle);
    }
}

static void StartTelemetryTask(void *argument)
{
    monitor_measurement_t measurement;

    char tx_buffer[128];

    const char startup_msg[] =
        "\r\nSTM32 Real-Time System Monitor\r\n"
        "ADC + DMA + FreeRTOS + UART\r\n"
        "Multitasking initialized.\r\n\r\n";

    Monitor_UART_Send(startup_msg);

    for (;;)
    {
        /*
         * Wait for a measurement from Sensor Task.
         */
        if (osMessageQueueGet(
                measurementQueueHandle,
                &measurement,
                NULL,
                osWaitForever) == osOK)
        {
            /*
             * Convert temperature into tenths
             * of a degree.
             */
            int32_t temp_tenths =
                (int32_t)(measurement.temperature_c
                          * 10.0f);

            int32_t temp_abs =
                (temp_tenths < 0) ?
                -temp_tenths : temp_tenths;

            /*
             * Format the telemetry message.
             */
            int len = snprintf(
                tx_buffer,
                sizeof(tx_buffer),
                "TEMP=%s%ld.%ld C, VDDA=%lu mV\r\n",
                (temp_tenths < 0) ? "-" : "",
                (long)(temp_abs / 10),
                (long)(temp_abs % 10),
                (unsigned long)measurement.vdda_mv
            );

            /*
             * Transmit only if formatting succeeded.
             */
            if ((len > 0) &&
                ((size_t)len < sizeof(tx_buffer)))
            {
                Monitor_UART_Send(tx_buffer);
            }
        }
    }
}

/* USER CODE END 4 */

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN 5 */

	monitor_measurement_t measurement;

	for (;;)
	{
	    /*
	     * Clear any previous ADC notification flags.
	     */
	    osThreadFlagsClear(
	        MONITOR_ADC_DONE_FLAG |
	        MONITOR_ADC_ERROR_FLAG
	    );

	    /*
	     * Start ADC acquisition using DMA.
	     */
	    if (Monitor_ADC_Start() != HAL_OK)
	    {
	        Monitor_UART_Send(
	            "ERROR: ADC start failed\r\n"
	        );

	        osDelay(1000);
	        continue;
	    }

	    /*
	     * Wait for DMA completion or ADC error.
	     *
	     * Timeout = 100 RTOS ticks.
	     */
	    uint32_t flags = osThreadFlagsWait(
	        MONITOR_ADC_DONE_FLAG |
	        MONITOR_ADC_ERROR_FLAG,
	        osFlagsWaitAny,
	        100
	    );

	    /*
	     * Check whether the wait failed or timed out.
	     */
	    if ((flags & osFlagsError) != 0U)
	    {
	        HAL_ADC_Stop_DMA(&hadc1);

	        Monitor_UART_Send(
	            "ERROR: ADC notification timeout\r\n"
	        );

	        osDelay(1000);
	        continue;
	    }

	    /*
	     * Check whether the ADC reported an error.
	     */
	    if ((flags & MONITOR_ADC_ERROR_FLAG) != 0U)
	    {
	        HAL_ADC_Stop_DMA(&hadc1);

	        Monitor_UART_Send(
	            "ERROR: ADC conversion failed\r\n"
	        );

	        osDelay(1000);
	        continue;
	    }

	    /*
	     * Check that DMA completed successfully.
	     */
	    if ((flags & MONITOR_ADC_DONE_FLAG) != 0U)
	    {
	        /*
	         * Stop the completed one-shot DMA.
	         */
	        HAL_ADC_Stop_DMA(&hadc1);

	        /*
	         * Convert raw ADC values.
	         */
	        Monitor_ADC_Convert(
	            &measurement.temperature_c,
	            &measurement.vdda_mv
	        );

	        /*
	         * Check for invalid reference measurement.
	         */
	        if (measurement.vdda_mv == 0U)
	        {
	            Monitor_UART_Send(
	                "ERROR: Invalid VREFINT reading\r\n"
	            );
	        }
	        else
	        {
	            /*
	             * Send measurement to Telemetry Task.
	             */
	            if (osMessageQueuePut(
	                    measurementQueueHandle,
	                    &measurement,
	                    0,
	                    10) != osOK)
	            {
	                Monitor_UART_Send(
	                    "ERROR: Measurement queue full\r\n"
	                );
	            }
	        }

	        /*
	         * Toggle onboard LED.
	         */
	        HAL_GPIO_TogglePin(
	            LD2_GPIO_Port,
	            LD2_Pin
	        );
	    }

	    /*
	     * Wait one second before next acquisition.
	     */
	    osDelay(1000);
	}

  /* USER CODE END 5 */
}

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM6 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM6)
  {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
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
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
