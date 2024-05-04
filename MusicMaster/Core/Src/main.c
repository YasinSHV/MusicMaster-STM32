/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2024 STMicroelectronics.
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

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define DEBOUNCE_DELAY 350

#define PASSWORD_LENGTH 4

uint8_t passwordTx[PASSWORD_LENGTH] = "1234";
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

SPI_HandleTypeDef hspi1;

TIM_HandleTypeDef htim1;
TIM_HandleTypeDef htim2;

UART_HandleTypeDef huart1;

PCD_HandleTypeDef hpcd_USB_FS;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_SPI1_Init(void);
static void MX_USB_PCD_Init(void);
static void MX_TIM1_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_TIM2_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
struct digit {
	int number;
	uint16_t pattern;
	uint16_t anti_pattern;
};

uint16_t led[4];

int carrier[] = { 0, 0, 0, 0 };

int number = 0;
int password[4] = {-1,-1,-1,-1};
int wrongPassCounter = 0;

char logCorrect[19] = "LOG(XXXX, CORRECT)\n";
char logFailed[18] = "LOG(XXXX, FAILED)\n";

int logStatus = 1;
int alertStatus = 1;

int displayed_number[4];

uint32_t previousMillis = 0;
uint32_t currentMillis = 0;
uint32_t prevTime = 0;
uint32_t coolDownTimer = 0;

int isCorrect = -1;
int nextTurn = 0;
int blink = 1;
int index = 0;

uint32_t prevEnterTime = 0;

uint32_t buzzerEnterTime = 0;

uint32_t s = 0, e = 0;

int LED = 0, counter = 0;

struct digit digits[10];

enum BuzzerFlag {
	CorrectInput, WrongInput, CorrectPass, WrongPass, SuperWrongPass, NONE
};
enum BuzzerFlag buzzer_flag = NONE;
uint32_t buzzerCoolDown = 0;
//PWM BEGIN
TIM_HandleTypeDef *pwm_timer = &htim2;
uint32_t pwm_channel = TIM_CHANNEL_1;
uint16_t _volume = 10;

void PWM_Start() {
	HAL_TIM_PWM_Start(pwm_timer, pwm_channel);
}
void PWM_Stop() {
	HAL_TIM_PWM_Stop(pwm_timer, pwm_channel);
}

void PWM_Change_Tone(uint16_t pwm_freq, uint16_t volume) //(1-20000), (0-1000)
{
	if (pwm_freq == 0 || pwm_freq > 20000)
		__HAL_TIM_SET_COMPARE(pwm_timer, pwm_channel, 0);
	else {
		const uint32_t internal_clock_freq = HAL_RCC_GetSysClockFreq();

		const uint16_t prescaler = 1 + internal_clock_freq / pwm_freq / 60000;
		const uint32_t timer_clock = internal_clock_freq / prescaler;
		const uint32_t period_cycles = timer_clock / pwm_freq;
		const uint32_t puls_width = volume * period_cycles / 1000 / 2;

		pwm_timer->Instance->PSC = prescaler - 1;
		pwm_timer->Instance->ARR = period_cycles - 1;
		pwm_timer->Instance->EGR = TIM_EGR_UG;
		__HAL_TIM_SET_COMPARE(pwm_timer, pwm_channel, puls_width);
	}
}
//PWM END

//UART BEGIN

void extractNumber(const uint8_t *data) {
	isCorrect = -1;
	// Extract the first four characters and convert them to integers
	char pass[19] = "PASS_CHANGED(XXXX)\n";
	pass[13] = data[9];
	pass[14] = data[10];
	pass[15] = data[11];
	pass[16] = data[12];

	for (int i = 9; i < 13; i++) {
		if (data[i] <= '9' && data[i] >= '0') {
			password[i - 9] = data[i] - '0';
		} else {
			if (logStatus) {
				HAL_UART_Transmit_IT(&huart1, "ERROR(INCORRECT FORMAT)\n", 24);
			}
			HAL_GPIO_WritePin(GPIOC, GPIO_PIN_0, 0);
			buzzer_flag = WrongInput;
			buzzerEnterTime = HAL_GetTick();
			if (alertStatus)
				PWM_Start();
			return;
		}
	}
	if (logStatus) {
		HAL_UART_Transmit_IT(&huart1, pass, 19);
		buzzer_flag = CorrectInput;
		buzzerEnterTime = HAL_GetTick();
		if (alertStatus)
			PWM_Start();
	}
}

int compareStrings(const char *str1, const uint8_t *str2, int n) {
	for (int i = 0; i < n; i++) {
		if (str1[i] != str2[i]) {
			return 0;
		}
	}
	return 1;
}

char setPass[9] = "SET_PASS(";
char logON[6] = "LOG_ON";
char logOFF[7] = "LOG_OFF";
char alertON[8] = "ALERT_ON";
char alertOFF[9] = "ALERT_OFF";
char setVolume[11] = "SET_VOLUME(";

uint8_t data[100];
uint8_t d;
uint8_t i;

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
	if (huart->Instance == USART1) {
		HAL_UART_Receive_IT(&huart1, &d, 1);
		data[i++] = d;
		if (d == '\n') {
			if ((i == 15 && compareStrings(setPass, data, 9) == 1)
					&& isCorrect == -1) {
				HAL_GPIO_WritePin(GPIOC, GPIO_PIN_0, 1);
				HAL_GPIO_WritePin(GPIOC, GPIO_PIN_2, 0);
				extractNumber(data);
			} else if (i == 7) {
				if (compareStrings(logON, data, 6) == 1) {
					logStatus = 1;
					HAL_UART_Transmit_IT(&huart1, "Program Log Turned ON\n",
							22);
				}
			} else if (i == 8) {
				if (compareStrings(logOFF, data, 7) == 1) {
					logStatus = 0;
					HAL_UART_Transmit_IT(&huart1, "Program Log Turned OFF\n",
							23);
				}
			} else if (i == 9) {
				if (compareStrings(alertON, data, 8) == 1) {
					alertStatus = 1;
					HAL_UART_Transmit_IT(&huart1, "Program Alerts Turned ON\n",
							25);
					buzzer_flag = CorrectInput;
					buzzerEnterTime = HAL_GetTick();
					PWM_Start();
				}
			} else if (i == 10) {
				if (compareStrings(alertOFF, data, 9) == 1) {
					alertStatus = 0;
					HAL_UART_Transmit_IT(&huart1, "Program Alerts Turned OFF\n",
							26);
				}
			} else if (i == 14 && (data[11] - '0') < 6
					&& (data[11] - '0') > -1) {
				if (compareStrings(setVolume, data, 11) == 1
						&& data[12] == ')') {
					int v = data[11] - '0';
					_volume = v * 10;
					char massage[24] = "Program Volume Set To  \n";
					massage[22] = data[11];
					HAL_UART_Transmit_IT(&huart1, massage, 24);
				}
			} else {
				HAL_UART_Transmit_IT(&huart1, "INVALID INPUT\n", 14);
				buzzer_flag = WrongInput;
				buzzerEnterTime = HAL_GetTick();
				if (alertStatus)
					PWM_Start();
			}

			i = 0;
		}

	}
}

//UART END

void display_number(int led_flag, int _number) {
	HAL_GPIO_WritePin(GPIOD,
	GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4, GPIO_PIN_SET);
	HAL_GPIO_WritePin(GPIOD, led[led_flag], GPIO_PIN_RESET);
	if (_number != 0) {
		HAL_GPIO_WritePin(GPIOD, digits[_number].pattern, GPIO_PIN_SET);
	}
	HAL_GPIO_WritePin(GPIOD, digits[_number].anti_pattern, GPIO_PIN_RESET);
}

void increase(int _head) {
	carrier[_head] += 1;
	if (carrier[_head] == 10) {
		carrier[_head] = 0;
	}
}

void init_display() {
	HAL_GPIO_WritePin(GPIOD,
	GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4, GPIO_PIN_SET);
}

/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void) {
	/* USER CODE BEGIN 1 */
	struct digit _digits[10];
	_digits[0].number = 0;
	_digits[0].anti_pattern = GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14
			| GPIO_PIN_15;

	_digits[1].number = 1;
	_digits[1].pattern = GPIO_PIN_12;
	_digits[1].anti_pattern = GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;

	_digits[2].number = 2;
	_digits[2].pattern = GPIO_PIN_13;
	_digits[2].anti_pattern = GPIO_PIN_12 | GPIO_PIN_14 | GPIO_PIN_15;

	_digits[3].number = 3;
	_digits[3].pattern = GPIO_PIN_12 | GPIO_PIN_13;
	_digits[3].anti_pattern = GPIO_PIN_14 | GPIO_PIN_15;

	_digits[4].number = 4;
	_digits[4].pattern = GPIO_PIN_14;
	_digits[4].anti_pattern = GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_15;

	_digits[5].number = 5;
	_digits[5].pattern = GPIO_PIN_12 | GPIO_PIN_14;
	_digits[5].anti_pattern = GPIO_PIN_13 | GPIO_PIN_15;

	_digits[6].number = 6;
	_digits[6].pattern = GPIO_PIN_13 | GPIO_PIN_14;
	_digits[6].anti_pattern = GPIO_PIN_12 | GPIO_PIN_15;

	_digits[7].number = 7;
	_digits[7].pattern = GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14;
	_digits[7].anti_pattern = GPIO_PIN_15;

	_digits[8].number = 8;
	_digits[8].pattern = GPIO_PIN_15;
	_digits[8].anti_pattern = GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14;

	_digits[9].number = 9;
	_digits[9].pattern = GPIO_PIN_12 | GPIO_PIN_15;
	_digits[9].anti_pattern = GPIO_PIN_13 | GPIO_PIN_14;

	for (int i = 0; i < 10; i++) {
		digits[i] = _digits[i];
	}

	/* USER CODE END 1 */

	/* MCU Configuration--------------------------------------------------------*/

	/* Reset of all peripherals, Initializes the Flash interface and the Systick. */
	HAL_Init();

	/* USER CODE BEGIN Init */

	led[0] = GPIO_PIN_1;
	led[1] = GPIO_PIN_2;
	led[2] = GPIO_PIN_3;
	led[3] = GPIO_PIN_4;

	/* USER CODE END Init */

	/* Configure the system clock */
	SystemClock_Config();

	/* USER CODE BEGIN SysInit */

	/* USER CODE END SysInit */

	/* Initialize all configured peripherals */
	MX_GPIO_Init();
	MX_I2C1_Init();
	MX_SPI1_Init();
	MX_USB_PCD_Init();
	MX_TIM1_Init();
	MX_USART1_UART_Init();
	MX_TIM2_Init();
	/* USER CODE BEGIN 2 */
	HAL_TIM_Base_Start_IT(&htim1);
	HAL_UART_Transmit_IT(&huart1,
			"=============\nProgram Running\n=============\n", 44);
	HAL_UART_Receive_IT(&huart1, &d, 1);
	/* USER CODE END 2 */

	/* Infinite loop */
	/* USER CODE BEGIN WHILE */
	while (1) {

		/* USER CODE END WHILE */

		/* USER CODE BEGIN 3 */

	}
	/* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void) {
	RCC_OscInitTypeDef RCC_OscInitStruct = { 0 };
	RCC_ClkInitTypeDef RCC_ClkInitStruct = { 0 };
	RCC_PeriphCLKInitTypeDef PeriphClkInit = { 0 };

	/** Initializes the RCC Oscillators according to the specified parameters
	 * in the RCC_OscInitTypeDef structure.
	 */
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI
			| RCC_OSCILLATORTYPE_HSE;
	RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;
	RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
	RCC_OscInitStruct.HSIState = RCC_HSI_ON;
	RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
	RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL6;
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
		Error_Handler();
	}

	/** Initializes the CPU, AHB and APB buses clocks
	 */
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
			| RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK) {
		Error_Handler();
	}
	PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USB
			| RCC_PERIPHCLK_USART1 | RCC_PERIPHCLK_I2C1 | RCC_PERIPHCLK_TIM1;
	PeriphClkInit.Usart1ClockSelection = RCC_USART1CLKSOURCE_PCLK2;
	PeriphClkInit.I2c1ClockSelection = RCC_I2C1CLKSOURCE_HSI;
	PeriphClkInit.USBClockSelection = RCC_USBCLKSOURCE_PLL;
	PeriphClkInit.Tim1ClockSelection = RCC_TIM1CLK_HCLK;
	if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK) {
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
	hi2c1.Init.Timing = 0x2000090E;
	hi2c1.Init.OwnAddress1 = 0;
	hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
	hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
	hi2c1.Init.OwnAddress2 = 0;
	hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
	hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
	hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
	if (HAL_I2C_Init(&hi2c1) != HAL_OK) {
		Error_Handler();
	}

	/** Configure Analogue filter
	 */
	if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE)
			!= HAL_OK) {
		Error_Handler();
	}

	/** Configure Digital filter
	 */
	if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN I2C1_Init 2 */

	/* USER CODE END I2C1_Init 2 */

}

/**
 * @brief SPI1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_SPI1_Init(void) {

	/* USER CODE BEGIN SPI1_Init 0 */

	/* USER CODE END SPI1_Init 0 */

	/* USER CODE BEGIN SPI1_Init 1 */

	/* USER CODE END SPI1_Init 1 */
	/* SPI1 parameter configuration*/
	hspi1.Instance = SPI1;
	hspi1.Init.Mode = SPI_MODE_MASTER;
	hspi1.Init.Direction = SPI_DIRECTION_2LINES;
	hspi1.Init.DataSize = SPI_DATASIZE_4BIT;
	hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
	hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
	hspi1.Init.NSS = SPI_NSS_SOFT;
	hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_4;
	hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
	hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
	hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
	hspi1.Init.CRCPolynomial = 7;
	hspi1.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
	hspi1.Init.NSSPMode = SPI_NSS_PULSE_ENABLE;
	if (HAL_SPI_Init(&hspi1) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN SPI1_Init 2 */

	/* USER CODE END SPI1_Init 2 */

}

/**
 * @brief TIM1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM1_Init(void) {

	/* USER CODE BEGIN TIM1_Init 0 */

	/* USER CODE END TIM1_Init 0 */

	TIM_ClockConfigTypeDef sClockSourceConfig = { 0 };
	TIM_MasterConfigTypeDef sMasterConfig = { 0 };

	/* USER CODE BEGIN TIM1_Init 1 */

	/* USER CODE END TIM1_Init 1 */
	htim1.Instance = TIM1;
	htim1.Init.Prescaler = 4800 - 1;
	htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
	htim1.Init.Period = 5 - 1;
	htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	htim1.Init.RepetitionCounter = 0;
	htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
	if (HAL_TIM_Base_Init(&htim1) != HAL_OK) {
		Error_Handler();
	}
	sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
	if (HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig) != HAL_OK) {
		Error_Handler();
	}
	sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
	sMasterConfig.MasterOutputTrigger2 = TIM_TRGO2_RESET;
	sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
	if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig)
			!= HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN TIM1_Init 2 */

	/* USER CODE END TIM1_Init 2 */

}

/**
 * @brief TIM2 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM2_Init(void) {

	/* USER CODE BEGIN TIM2_Init 0 */

	/* USER CODE END TIM2_Init 0 */

	TIM_ClockConfigTypeDef sClockSourceConfig = { 0 };
	TIM_MasterConfigTypeDef sMasterConfig = { 0 };
	TIM_OC_InitTypeDef sConfigOC = { 0 };

	/* USER CODE BEGIN TIM2_Init 1 */

	/* USER CODE END TIM2_Init 1 */
	htim2.Instance = TIM2;
	htim2.Init.Prescaler = 0;
	htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
	htim2.Init.Period = 4294967295;
	htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
	if (HAL_TIM_Base_Init(&htim2) != HAL_OK) {
		Error_Handler();
	}
	sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
	if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK) {
		Error_Handler();
	}
	if (HAL_TIM_PWM_Init(&htim2) != HAL_OK) {
		Error_Handler();
	}
	sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
	sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
	if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig)
			!= HAL_OK) {
		Error_Handler();
	}
	sConfigOC.OCMode = TIM_OCMODE_PWM1;
	sConfigOC.Pulse = 0;
	sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
	sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
	if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_1)
			!= HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN TIM2_Init 2 */

	/* USER CODE END TIM2_Init 2 */
	HAL_TIM_MspPostInit(&htim2);

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
	huart1.Init.BaudRate = 115200;
	huart1.Init.WordLength = UART_WORDLENGTH_8B;
	huart1.Init.StopBits = UART_STOPBITS_1;
	huart1.Init.Parity = UART_PARITY_NONE;
	huart1.Init.Mode = UART_MODE_TX_RX;
	huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	huart1.Init.OverSampling = UART_OVERSAMPLING_16;
	huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
	huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
	if (HAL_UART_Init(&huart1) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN USART1_Init 2 */

	/* USER CODE END USART1_Init 2 */

}

/**
 * @brief USB Initialization Function
 * @param None
 * @retval None
 */
static void MX_USB_PCD_Init(void) {

	/* USER CODE BEGIN USB_Init 0 */

	/* USER CODE END USB_Init 0 */

	/* USER CODE BEGIN USB_Init 1 */

	/* USER CODE END USB_Init 1 */
	hpcd_USB_FS.Instance = USB;
	hpcd_USB_FS.Init.dev_endpoints = 8;
	hpcd_USB_FS.Init.speed = PCD_SPEED_FULL;
	hpcd_USB_FS.Init.phy_itface = PCD_PHY_EMBEDDED;
	hpcd_USB_FS.Init.low_power_enable = DISABLE;
	hpcd_USB_FS.Init.battery_charging_enable = DISABLE;
	if (HAL_PCD_Init(&hpcd_USB_FS) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN USB_Init 2 */

	/* USER CODE END USB_Init 2 */

}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void) {
	GPIO_InitTypeDef GPIO_InitStruct = { 0 };

	/* GPIO Ports Clock Enable */
	__HAL_RCC_GPIOE_CLK_ENABLE();
	__HAL_RCC_GPIOC_CLK_ENABLE();
	__HAL_RCC_GPIOF_CLK_ENABLE();
	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOD_CLK_ENABLE();
	__HAL_RCC_GPIOB_CLK_ENABLE();

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(CS_I2C_SPI_GPIO_Port, CS_I2C_SPI_Pin, GPIO_PIN_RESET);

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2,
			GPIO_PIN_RESET);

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(GPIOD,
			GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15 | GPIO_PIN_1
					| GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4, GPIO_PIN_RESET);


	/*Configure GPIO pin : CS_I2C_SPI_Pin */
	GPIO_InitStruct.Pin = CS_I2C_SPI_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(CS_I2C_SPI_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pins : PC0 PC1 PC2 */
	GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

	/*Configure GPIO pin : PA0 */
	GPIO_InitStruct.Pin = GPIO_PIN_0;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	/*Configure GPIO pins : PA1 PA3 */
	GPIO_InitStruct.Pin = GPIO_PIN_1 | GPIO_PIN_3;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
	GPIO_InitStruct.Pull = GPIO_PULLDOWN;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	/*Configure GPIO pins : PD12 PD13 PD14 PD15
	 PD1 PD2 PD3 PD4 */
	GPIO_InitStruct.Pin = GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15
			| GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

	/* EXTI interrupt init*/
	HAL_NVIC_SetPriority(EXTI0_IRQn, 0, 0);
	HAL_NVIC_EnableIRQ(EXTI0_IRQn);

	HAL_NVIC_SetPriority(EXTI1_IRQn, 0, 0);
	HAL_NVIC_EnableIRQ(EXTI1_IRQn);

	HAL_NVIC_SetPriority(EXTI3_IRQn, 0, 0);
	HAL_NVIC_EnableIRQ(EXTI3_IRQn);

}

/* USER CODE BEGIN 4 */

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
	currentMillis = HAL_GetTick();
	if ((GPIO_Pin == GPIO_PIN_1) && LED != 4) {
		if ((currentMillis - previousMillis > DEBOUNCE_DELAY)) {
//			counterInside++;
			increase(LED);
			previousMillis = currentMillis;
		}
	} else if ((GPIO_Pin == GPIO_PIN_3)) {

		if ((currentMillis - previousMillis > DEBOUNCE_DELAY)) {
			if (nextTurn < 6)
				nextTurn++;
			if ((isCorrect == -1) && nextTurn == 6) {
				for (int i = 0; i < 4; i++) {
					carrier[i] = 0;
				    //password[i] = 0;
				}
				//HAL_GPIO_WritePin(GPIOC, GPIO_PIN_0, 0);
				HAL_GPIO_WritePin(GPIOC, GPIO_PIN_1, 0);
				HAL_GPIO_WritePin(GPIOC, GPIO_PIN_2, 0);
				LED = -1;
				nextTurn = 0;
				blink = 1;
				return;
			} else if ((isCorrect != 0 && isCorrect != 1)) {

				LED += 1;
				if (LED == 4) {
					int bool = 1;
					for (int i = 0; i < 4; i++) {
						if (password[i] != carrier[i]) {
							bool = 0;
							logFailed[4] = carrier[0] + '0';
							logFailed[5] = carrier[1] + '0';
							logFailed[6] = carrier[2] + '0';
							logFailed[7] = carrier[3] + '0';
							isCorrect = 0;
							if (logStatus)
								HAL_UART_Transmit_IT(&huart1, logFailed, 18);
							prevEnterTime = HAL_GetTick();
							wrongPassCounter++;
							if (wrongPassCounter % 3 == 0)
								buzzer_flag = SuperWrongPass;
							else
								buzzer_flag = WrongPass;
							buzzerEnterTime = HAL_GetTick();
							if (alertStatus)
								PWM_Start();
							return;
						}
					}
					if (bool) {
						logCorrect[4] = carrier[0] + '0';
						logCorrect[5] = carrier[1] + '0';
						logCorrect[6] = carrier[2] + '0';
						logCorrect[7] = carrier[3] + '0';
						isCorrect = 1;
						if (logStatus)
							HAL_UART_Transmit_IT(&huart1, logCorrect, 19);
						prevEnterTime = HAL_GetTick();
						buzzer_flag = CorrectPass;
						buzzerEnterTime = HAL_GetTick();
						if (alertStatus)
							PWM_Start();
						wrongPassCounter = 0;
						return;
					}
				}
			}
			previousMillis = currentMillis;
		}

	} else if (GPIO_Pin == GPIO_PIN_0 && isCorrect == 0) {
		blink = 0;
		isCorrect = -1;
		prevEnterTime = HAL_GetTick();
		PWM_Stop();
		buzzer_flag = NONE;
	}
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
	if (htim->Instance == TIM1) {
		if (index == LED && HAL_GetTick() - prevTime < 400) {
			display_number(index, carrier[index]);
			coolDownTimer = HAL_GetTick();
		} else if (index == LED && HAL_GetTick() - prevTime > 900) {
			//coolDown
			if (HAL_GetTick() - coolDownTimer > 100) {
				prevTime = HAL_GetTick();
			}
		} else if (index != LED) {
			display_number(index, carrier[index]);
		}
		if (index == 4) {
			index = 0;
			init_display();
		} else {
			++index;
		}

		if (isCorrect == 1 && blink == 1) {
			HAL_GPIO_WritePin(GPIOC, GPIO_PIN_1, 1);
			if (HAL_GetTick() - prevEnterTime > 5000) {
				blink = 0;
				HAL_GPIO_WritePin(GPIOC, GPIO_PIN_1, 0);
				prevEnterTime = HAL_GetTick();
				isCorrect = -1;
			}
		}

		if (isCorrect == 0 && blink == 1) {
			HAL_GPIO_WritePin(GPIOC, GPIO_PIN_2, 0);
			if (HAL_GetTick() - prevEnterTime < 5000) {
				if (HAL_GetTick() - prevTime > 100) {
					HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_2);
					prevTime = HAL_GetTick();
				}
			} else {
				if (buzzer_flag != SuperWrongPass) {
					blink = 0;
					isCorrect = -1;
					prevEnterTime = HAL_GetTick();
				}
			}
		}

		switch (buzzer_flag) {
		case CorrectInput:
			if (buzzer_flag == CorrectInput) {
				if (HAL_GetTick() - buzzerEnterTime < 50) {
					PWM_Change_Tone(507, _volume);
				} else {
					PWM_Stop();
					buzzer_flag = NONE;
				}
			}
			break;
		case WrongInput:
			if (HAL_GetTick() - buzzerEnterTime < 1000) {
				if (HAL_GetTick() - buzzerCoolDown < 50) {
					PWM_Change_Tone(236, _volume);
				} else if (HAL_GetTick() - buzzerCoolDown < 300) {
					//coolDown
					PWM_Change_Tone(0, 0);
				} else
					buzzerCoolDown = HAL_GetTick();
			} else {
				PWM_Stop();
				buzzer_flag = NONE;
			}
			break;
		case CorrectPass:
			if (HAL_GetTick() - buzzerEnterTime < 1200) {
				if (HAL_GetTick() - buzzerEnterTime < 800) {
					if (HAL_GetTick() - buzzerCoolDown < 50) {
						PWM_Change_Tone(1000, _volume);
					} else if (HAL_GetTick() - buzzerCoolDown < 100) {
						//coolDown
						PWM_Change_Tone(0, _volume);
					} else
						buzzerCoolDown = HAL_GetTick();
				} else {
					PWM_Change_Tone(1000, _volume);
				}
			} else {
				PWM_Stop();
				buzzer_flag = NONE;
			}
			break;
		case WrongPass:
			if (HAL_GetTick() - buzzerEnterTime < 1500) {
				if (HAL_GetTick() - buzzerCoolDown < 150) {
					PWM_Change_Tone(700, _volume);
				} else if (HAL_GetTick() - buzzerCoolDown < 450) {
					//coolDown
					PWM_Change_Tone(0, _volume);
				} else
					buzzerCoolDown = HAL_GetTick();
			} else {
				PWM_Stop();
				buzzer_flag = NONE;
			}
			break;
		case SuperWrongPass:
			PWM_Change_Tone(1000, _volume);
			break;
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
	/* User can add his own implementation to report the HAL error return state */
	__disable_irq();
	while (1) {
	}
	/* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
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
