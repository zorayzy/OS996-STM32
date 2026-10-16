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
#include "bsp_gxht30.h"
#include "bsp_pir.h"
#include "bsp_mq2.h"
#include "app_data.h"
#include "bsp_alarm.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc1;

I2C_HandleTypeDef hi2c1;

TIM_HandleTypeDef htim3;

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;
DMA_HandleTypeDef hdma_usart2_rx;
DMA_HandleTypeDef hdma_usart2_tx;

/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for SensorTask */
osThreadId_t SensorTaskHandle;
const osThreadAttr_t SensorTask_attributes = {
  .name = "SensorTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityNormal1,
};
/* Definitions for CommTask */
osThreadId_t CommTaskHandle;
const osThreadAttr_t CommTask_attributes = {
  .name = "CommTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityAboveNormal1,
};
/* Definitions for DisoalyTask */
osThreadId_t DisoalyTaskHandle;
const osThreadAttr_t DisoalyTask_attributes = {
  .name = "DisoalyTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityLow,
};
/* Definitions for AlarmTask */
osThreadId_t AlarmTaskHandle;
const osThreadAttr_t AlarmTask_attributes = {
  .name = "AlarmTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityNormal1,
};
/* USER CODE BEGIN PV */

/* 传感器数据队列：SensorTask 发送 → AlarmTask 接收 */
osMessageQueueId_t SensorQueueHandle;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_ADC1_Init(void);
static void MX_I2C1_Init(void);
static void MX_TIM3_Init(void);
void StartDefaultTask(void *argument);
void StartSensorTask(void *argument);
void StartCommTask(void *argument);
void StartDisplayTask(void *argument);
void startAlarmtask(void *argument);

/* USER CODE BEGIN PFP */

/* 用户函数声明（避免隐式声明警告） */
void i2c_scan(void);

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
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();
  MX_ADC1_Init();
  MX_I2C1_Init();
  MX_TIM3_Init();
  /* USER CODE BEGIN 2 */
  printf("\r\n");
  printf("========================================\r\n");
  printf("  OS996 Smart Home Project\r\n");
  printf("  Build: %s %s\r\n", __DATE__, __TIME__);
  printf("========================================\r\n");
  printf("  SYSCLK  = %lu Hz\r\n", (unsigned long)SystemCoreClock);
  printf("  HAL tick= %lu\r\n", (unsigned long)HAL_GetTick());

   /* 启动 ADC + DMA（必须在 osKernelStart() 之前！）*/
  mq2_init();
  printf("  ADC1+DMA started (MQ-2 gas sensor)\r\n");

  /* 启动时扫描一次 I2C 总线，确认 SHT30 在线 */
  //i2c_scan();

  /* USER CODE END 2 */

  /* Init scheduler */
  osKernelInitialize();

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* ------------------------------------------------------------------
   * 创建传感器数据队列
   * ------------------------------------------------------------------
   * 参数 1：最多存几个元素 —— 3 个足够（生产者每 50ms 一个，
   *         消费者处理只需几十微秒，几乎不会堆积）
   * 参数 2：每个元素的字节数 —— ★ 用 sizeof 让编译器算，不可能写错
   * 参数 3：NULL = 动态分配
   * ------------------------------------------------------------------ */
  SensorQueueHandle = osMessageQueueNew(3, sizeof(sensor_data_t), NULL);
  if (SensorQueueHandle == NULL) {
      Error_Handler();          /* 堆不够时返回 NULL，必须判空 */
  }
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* creation of SensorTask */
  SensorTaskHandle = osThreadNew(StartSensorTask, NULL, &SensorTask_attributes);

  /* creation of CommTask */
  CommTaskHandle = osThreadNew(StartCommTask, NULL, &CommTask_attributes);

  /* creation of DisoalyTask */
  DisoalyTaskHandle = osThreadNew(StartDisplayTask, NULL, &DisoalyTask_attributes);

  /* creation of AlarmTask */
  AlarmTaskHandle = osThreadNew(startAlarmtask, NULL, &AlarmTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
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
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
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

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.ScanConvMode = ENABLE;
  hadc1.Init.ContinuousConvMode = ENABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 2;
  hadc1.Init.DMAContinuousRequests = ENABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_4;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_84CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_5;
  sConfig.Rank = 2;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

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
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 83;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 999;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_PWM_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_3) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_4) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */
  HAL_TIM_MspPostInit(&htim3);

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
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
  __HAL_RCC_DMA2_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Stream5_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream5_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream5_IRQn);
  /* DMA1_Stream6_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream6_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream6_IRQn);
  /* DMA2_Stream0_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Stream0_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);

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
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(BEEP_GPIO_Port, BEEP_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOF, LED0_Pin|LED1_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin : PIR_Pin */
  GPIO_InitStruct.Pin = PIR_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(PIR_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : DOOR_Pin */
  GPIO_InitStruct.Pin = DOOR_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(DOOR_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : BEEP_Pin LED0_Pin LED1_Pin */
  GPIO_InitStruct.Pin = BEEP_Pin|LED0_Pin|LED1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
//#include <stdio.h>

/**
  * @brief  重定向 printf 到 USART1（板载 CH340）
  * @note   syscalls.c 里的 _write() 会调用这个函数，
  *         所以 printf / puts / fwrite 全都会走 USART1
  */
int __io_putchar(int ch)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, 0xFFFF);
    return ch;
}

/**
  * @brief  扫描 I2C1 总线上所有有应答的设备地址
  * ------------------------------------------------------------------
  * 原理：
  *   I2C 的 7 位地址空间是 1~127。对每个地址尝试一次"空写操作"
  *   （HAL_I2C_IsDeviceReady），如果从机在第 9 位拉低 SDA 应答，
  *   就说明这个地址上有设备。
  *
  * 为什么先写这个而不是直接写 SHT30 驱动：
  *   扫描器不依赖任何具体芯片，能独立验证【接线 / 供电 / 上拉 / 地址】。
  *   扫得到 -> I2C 外设配置正确，之后出错一定是驱动问题。
  *   扫不到 -> 硬件问题，不用去怀疑代码。
  *
  * 本板已知设备：
  *   0x44 = SHT30 温湿度
  *   0x3C = OLED SSD1306（若已接）
  *   0x50 = 板载 24C02 EEPROM
  * ------------------------------------------------------------------
  */
void i2c_scan(void)
{
    #define SCAN_PASSES   3          /* 扫描遍数 */

    uint8_t hits[128] = {0};         /* 每个地址被命中的次数 */
    uint8_t found = 0;

    printf("\r\n=== I2C1 bus scan (SCL=PB8, SDA=PB9) ===\r\n");
    printf("  scanning %u passes, only addresses that answer EVERY time are real\r\n",
           SCAN_PASSES);

    for (uint8_t pass = 0; pass < SCAN_PASSES; pass++)
    {
        for (uint8_t addr = 1; addr < 128; addr++)
        {
            /* 第 2 个参数要 8 位地址（7 位地址左移 1 位，最低位是 R/W） */
            /* 第 3 个参数 = 尝试次数，第 4 个 = 每次超时(ms) */
            if (HAL_I2C_IsDeviceReady(&hi2c1, (uint16_t)(addr << 1), 2, 10) == HAL_OK)
            {
                hits[addr]++;
            }
        }
        osDelay(50);                 /* 两遍之间歇一下 */
    }

    /* 只报告每次都有应答的地址 —— 那才是真实设备 */
    printf("  --- confirmed devices ---\r\n");
    for (uint8_t addr = 1; addr < 128; addr++)
    {
        if (hits[addr] == SCAN_PASSES)
        {
            printf("  0x%02X  (7-bit)   answered %u/%u\r\n",
                   addr, hits[addr], SCAN_PASSES);
            found++;
        }
    }

    /* 偶尔应答的，单独列出来 —— 这些是总线误判或干扰 */
    printf("  --- unreliable (ghost) addresses ---\r\n");
    {
        uint8_t ghost = 0;
        for (uint8_t addr = 1; addr < 128; addr++)
        {
            if (hits[addr] > 0 && hits[addr] < SCAN_PASSES)
            {
                printf("  0x%02X  (7-bit)   answered %u/%u  <-- NOT a real device\r\n",
                       addr, hits[addr], SCAN_PASSES);
                ghost++;
            }
        }
        if (ghost == 0) printf("  (none - bus is clean)\r\n");
    }

    if (found == 0)
    {
        printf("  NO confirmed device!\r\n");
        printf("  check: VCC=3.3V / SCL=PB8 / SDA=PB9 / GND\r\n");
    }
    else
    {
        printf("  total: %u confirmed device(s)\r\n", found);
    }

    printf("=== scan done ===\r\n\r\n");
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
  /* ------------------------------------------------------------------
   * V1 验证：LED 闪烁测试
   * ------------------------------------------------------------------
   * 目的：验证「代码真的在跑 + GPIO 输出正常 + 时钟 / FreeRTOS 正常」
   *
   * 硬件（正点原子探索者 V3）：
   *   PF9  -> DS0（红色 LED）
   *   PF10 -> DS1（绿色 LED）
   *   均为低电平点亮（active-low），所以：
   *       HAL_GPIO_WritePin(..., GPIO_PIN_RESET)  -> 亮
   *       HAL_GPIO_WritePin(..., GPIO_PIN_SET)    -> 灭
   *
   * 现象：两个 LED 以 500ms 为半周期交替闪烁
   *   t=0.0s  DS0 亮  DS1 灭
   *   t=0.5s  DS0 灭  DS1 亮
   *   t=1.0s  DS0 亮  DS1 灭   ... 循环
   *
   * 注意：必须用 osDelay()，不能用 HAL_Delay()
   *       —— HAL 的时基已经换成 TIM6，但仍然应该让出 CPU 给其他任务
   * ------------------------------------------------------------------ */

  printf("[DefaultTask] started,stack free = %u words\r\n",
        (unsigned)uxTaskGetStackHighWaterMark(NULL));

  for(;;)
  {

    osDelay(1000);   /* 1000ms */
  }
  /* USER CODE END 5 */
}

/* USER CODE BEGIN Header_StartSensorTask */
/**
* @brief Function implementing the SensorTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartSensorTask */
void StartSensorTask(void *argument)
{
  /* USER CODE BEGIN StartSensorTask */
  // Infinite loop 
  uint16_t gas_raw = 0;
  //float    gas_v = 0.0f;
  uint16_t gas_flt = 0;
  float    temp = 0.0f;
  float    humi = 0.0f;
  int      pir_state = PIR_NO_MOTION;
  uint32_t tick_cnt = 0;
  uint32_t ok = 0, err_i2c = 0, err_crc = 0;   // 统计三种结果出现次数 
  sensor_data_t tx;                            /* 要发送的数据 */
  //uint16_t gas_raw_local = 0;                  

  printf("[SensorTask] started, stack free = %lu words\r\n",
       (unsigned long)uxTaskGetStackHighWaterMark(NULL));

   /* ================================================================
   * PIR 预热阶段
   * ================================================================
   * HC-SR501 手册："通电后有一分钟左右的初始化时间，在此期间模块会
   * 间隔地输出 0-3 次，一分钟后进入待机状态。"
   *
   * 所以这 60 秒内必须【丢弃】PIR 的读数，否则会误报"有人"。
   * 注意这里仍然调用 pir_read() —— 让内部状态机正常运转，
   * 只是不打印结果。
   * ================================================================ */

  printf("[SensorTask] PIR warming up, please wait 60s...\r\n");

  /*for (uint32_t i = 0; i < 60; i++)
  {
      pir_read();          // 让状态机跑，但结果不输出
      osDelay(1000);
  }

  //预热结束，重置状态
  pir_reset();
  pir_state = PIR_NO_MOTION;*/

  uint32_t warmup_cnt = 0;
  int warming_up = 1;

  for(;;)
  {
    if(warming_up){
      pir_read();
      if(++warmup_cnt >= 1200){
        warming_up = 0;
        pir_reset();
        printf("[SensorTask] PIR ready\r\n");
        }
      }else{
        //50ms读一次PIR
        int now = pir_read();
        if(now != pir_state){
        pir_state = now;
        printf("[PIR] %s\r\n",
               pir_state ? "motion DETECTED" : "motion cleared");
        }
      }

    /* ---- ★ 每 50ms 组装并发送一次数据给 AlarmTask ---- */
    tx.temp    = temp;
    tx.humi    = humi;
    tx.pir     = (uint8_t)pir_state;
    tx.gas_raw = mq2_read_raw();
    tx.gas_v   = mq2_read_voltage();
    tx.tick    = HAL_GetTick();

    /* 超时 0 = 不等待。队列满就丢弃这一条（对周期数据合理） */
    osMessageQueuePut(SensorQueueHandle, &tx, 0, 0);

     //2s读一次温湿度
     if (++tick_cnt >= 40)      // 40 × 50ms = 2000ms
    {
      tick_cnt = 0;

      int ret = gxht30_read(&temp, &humi);

      gas_raw = mq2_read_raw();
      gas_flt = mq2_read();
      //gas_v   = mq2_read_voltage();

      if (ret == GXHT30_OK)
      {
        ok++;
        int32_t t100 = (int32_t)(temp * 100.0f);
        int32_t h100 = (int32_t)(humi * 100.0f);

        printf("[DATA] T=%ld.%02ld C  H=%ld.%02ld %%RH  PIR=%s GAS=%ldmV (raw=%u flt=%u)  (ok=%lu i2c=%lu crc=%lu)\r\n",
               (long)(t100 / 100), (long)(t100 % 100),
               (long)(h100 / 100), (long)(h100 % 100),
               warming_up ? "warmup" : (pir_state ? "1" : "0"),
               (long)(tx.gas_v * 1000.0f),      /* 电压转成 mV，用整数打印 */
               (unsigned)gas_raw,(unsigned)gas_flt,
               (unsigned long)ok, (unsigned long)err_i2c, (unsigned long)err_crc);
      }
      else if (ret == GXHT30_ERR_I2C)
      {
        err_i2c++;
        printf("[DATA] I2C ERROR  (ok=%lu i2c=%lu crc=%lu)\r\n",
               (unsigned long)ok, (unsigned long)err_i2c, (unsigned long)err_crc);
      }
      else
      {
        err_crc++;
        printf("[DATA] CRC ERROR  (ok=%lu i2c=%lu crc=%lu)\r\n",
               (unsigned long)ok, (unsigned long)err_i2c, (unsigned long)err_crc);
      }
    }  
    osDelay(50);
  }
  /* USER CODE END StartSensorTask */
}

/* USER CODE BEGIN Header_StartCommTask */
/**
* @brief Function implementing the CommTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartCommTask */
void StartCommTask(void *argument)
{
  /* USER CODE BEGIN StartCommTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartCommTask */
}

/* USER CODE BEGIN Header_StartDisplayTask */
/**
* @brief Function implementing the DisoalyTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartDisplayTask */
void StartDisplayTask(void *argument)
{
  /* USER CODE BEGIN StartDisplayTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartDisplayTask */
}

/* USER CODE BEGIN Header_startAlarmtask */
/**
* @brief Function implementing the AlarmTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_startAlarmtask */
void startAlarmtask(void *argument)
{
  /* USER CODE BEGIN startAlarmtask */
  /* Infinite loop */
  /* ==================================================================
   * 告警任务 —— 场景联动
   * ==================================================================
   * 职责：
   *   ① 【独占】LED0 / LED1 / 蜂鸣器 三个执行器（其他任务不碰）
   *   ② 从 SensorQueue 接收传感器数据
   *   ③ 判断报警条件，驱动执行器
   *
   * 报警条件：气体电压 > 阈值  且  PIR 检测到有人
   *
   * 为什么要"去抖"：
   *   报警条件可能在阈值附近快速反复变化 → LED/蜂鸣器会抖动。
   *   用"连续 N 次满足才成立"来消抖，和 PIR 的消抖是同一个思路。
   * ================================================================== */

  #define GAS_ALARM_THRESHOLD_MV   1000      /* ★ 报警阈值，单位 mV */
  #define ALARM_DEBOUNCE_CNT       6         /* 连续 6 次满足才认定 */
  #define ALARM_EVAL_PERIOD_MS     50        /* 评估周期 */

  sensor_data_t rx;
  int      alarm_on     = 0;                   /* 当前报警状态 */
  uint8_t  cond_cnt     = 0;                   /* 条件连续满足次数 */
  uint32_t blink_cnt    = 0;                   /* 报警闪烁计数 */

  alarm_all_off();                             /* 上电先全部关闭 */

  printf("[AlarmTask] started, stack free = %lu words\r\n",
         (unsigned long)uxTaskGetStackHighWaterMark(NULL));
  printf("[AlarmTask] threshold = %d mV, need PIR=1 AND gas above threshold\r\n",
         GAS_ALARM_THRESHOLD_MV);

  for(;;)
  {
    /* ---- 1) 从队列收数据（最多等 50ms）---- */
    if (osMessageQueueGet(SensorQueueHandle, &rx, NULL, ALARM_EVAL_PERIOD_MS) == osOK)
    {
      /* ---- 2) 判断报警条件 ---- */
      int cond = (rx.gas_v * 1000.0f > GAS_ALARM_THRESHOLD_MV) && (rx.pir == 1);

      if (cond) {
          if (cond_cnt < ALARM_DEBOUNCE_CNT) cond_cnt++;
      } else {
          cond_cnt = 0;                        /* 条件不成立，计数清零 */
      }

      int want_alarm = (cond_cnt >= ALARM_DEBOUNCE_CNT);

      /* ---- 3) 状态变化时打印一次（避免刷屏）---- */
      if (want_alarm != alarm_on)
      {
          alarm_on = want_alarm;
          if (alarm_on) {
              printf("[ALARM] *** ON ***  gas=%ldmV pir=%u\r\n",
                     (long)(rx.gas_v * 1000.0f), rx.pir);
          } else {
              printf("[ALARM] --- OFF ---  gas=%ldmV pir=%u\r\n",
                     (long)(rx.gas_v * 1000.0f), rx.pir);
              alarm_all_off();
          }
      }
    }
    else
    {
      /* 超时没收到数据（不应该发生）——当作正常评估一次超时处理 */
    }

    /* ---- 4) 驱动执行器 ---- */
    if (alarm_on)
    {
      /* 报警状态：LED0 + 蜂鸣器 以 250ms 为半周期闪烁鸣叫 */
      blink_cnt++;
      uint8_t phase = (uint8_t)((blink_cnt / 5) % 2);   /* 5×50ms = 250ms */

      alarm_led_red(phase);
      alarm_beep(phase);
      alarm_led_green(0);                   /* 绿灭 */
    }
    else
    {
      /* 正常状态：绿灯常亮表示"系统运行中"，红灯和蜂鸣器灭 */
      blink_cnt = 0;
      alarm_led_red(0);
      alarm_beep(0);
      alarm_led_green(1);
    }
  }
  /* USER CODE END startAlarmtask */
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
