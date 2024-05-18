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
#include "music_library.h"
#include "dictionary.h"
#include "stdlib.h"
#include "time.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define DEBOUNCE_DELAY 350
//Use Define to set when the 7-segment is on or off
//(to support both:{common anode, common cathode)
#define DISPLAY_ON GPIO_PIN_RESET
#define DISPLAY_OFF GPIO_PIN_SET
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;

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
static void MX_TIM2_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_ADC1_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
struct digit {
	int number;
	uint16_t pattern;
	uint16_t anti_pattern;
};
extern struct Dictionary *playlist;
extern char **playlistOrder;
int playedCount = 0, currentMusic = 0, adc_select = 0;
int index = 0;

extern volatile uint16_t volume;
struct Tone *melody;

uint16_t led[4];

int carrier[] = { 0, 0, 0, 0 };
int random_number;

uint32_t timePassed;
uint32_t previousMillis = 0;
uint32_t currentMillis = 0;

struct digit digits[10];

enum ProgramState {
	Paused, Resume, IDLE
};
enum ProgramState programState = Paused;

enum ProgramMode {
	Shuffle, Liner
};
enum ProgramMode programMode = Liner;

//UART BEGIN
//Time since program start
char* get_current_time() {
	static char time_str[6];

	// Convert milliseconds to seconds
	uint32_t seconds = timePassed / 1000;
	// Calculate minutes
	uint32_t minutes = seconds / 60;

	snprintf(time_str, sizeof(time_str), "%02d:%02d", minutes % 60,
			seconds % 60);

	return time_str;
}
void sendInfo(char *message, int *helper, int num, int number_size) {
	if (number_size == 0)
		number_size = 1;

	int message_len = strlen(message);
	message[message_len++] = ' ';
	for (int i = 0; i < number_size; i++) {
		message[message_len++] = helper[i] + '0';
	}
	message[message_len++] = ']';
	message[message_len++] = '[';
	message[message_len] = '\0';
	char *time_str = get_current_time();
	for (int i = 0; i < 5; i++) {
		message[message_len++] = time_str[i];
	}
	message[message_len++] = ']';
	message[message_len++] = '\n';
	message[message_len] = '\0';

	HAL_UART_Transmit(&huart1, message, message_len, 100);
}

void sendError(int changeMusic) {
	char error_message[34];
	char *time_str = get_current_time();

	if (changeMusic) {
		strcpy(error_message, "[ERROR] [Music not found][     ]\n");
		for (int i = 0; i < 5; i++) {
			error_message[strlen(error_message) - 7 + i] = time_str[i];
		}
		HAL_UART_Transmit_IT(&huart1, error_message, 33);
	} else {
		strcpy(error_message, "[ERROR] [Volume not Valid][     ]\n");
		for (int i = 0; i < 5; i++) {
			error_message[strlen(error_message) - 7 + i] = time_str[i];
		}
		HAL_UART_Transmit_IT(&huart1, error_message, 34);
	}
}

//Function to extract music number from Set_Music()
void extractNumber(const uint8_t *data) {
	int helper[digit_count(getDictSize(playlist))];
	int flag = 0;
	if (data[10 + digit_count(getDictSize(playlist))] == ')'
			&& (data[10] != '0')) {
		for (int i = 10; i < 10 + digit_count(getDictSize(playlist)); i++) {
			if (data[i] <= '9' && data[i] >= '0') {
				helper[i - 10] = data[i] - '0';
			} else {
				flag = 1;
				break;
			}
		}
		if (!flag) {
			int num = array_to_number(helper,
					digit_count(getDictSize(playlist)));
			if (num <= getDictSize(playlist)) {
				set_music(num);
				char message[100] = "[INFO][Music changed to ";
				sendInfo(message, helper, num, digit_count(num));
				return;
			}
		}
	}
	sendError(1);
//failed
}

void extractVolume(const uint8_t *data) {
	int helper[strlen(data) - 13];
	int flag = 0;
	for (int i = 11; i < strlen(data) - 2; i++) {
		if (data[i] <= '9' && data[i] >= '0') {
			helper[i - 11] = data[i] - '0';
		} else {
			flag = 1;
			break;
		}
	}
	if (!flag) {
		int num = array_to_number(helper, strlen(data) - 13);

		if (num < 101 && num > -1) {
			volume = num;
			char message[100] = "[INFO][Volume changed to ";
			sendInfo(message, helper, num, digit_count(num));
			return;
		}
	}
	sendError(0);
//failed
}

int compareStrings(const char *str1, const uint8_t *str2, int n) {
	for (int i = 0; i < n; i++) {
		if (str1[i] != str2[i]) {
			return 0;
		}
	}
	return 1;
}

char pause[5] = "pause";
char resume[6] = "resume";
char setMusic[10] = "set_music(";
char setVolume[11] = "set_volume(";

char setShuffle[18] = "play_mode(shuffle)";
char setLiner[18] = "play_mode(ordered)";

uint8_t data[100];
uint8_t d;
uint8_t i;

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
	if (huart->Instance == USART1) {
		HAL_UART_Receive_IT(&huart1, &d, 1);
		data[i++] = d;
		if (d == '\n') {
			data[i] = '\0';
			int len = strlen(data);
			if ((i == 6 && compareStrings(pause, data, 5) == 1)) {
				programState = Paused;
				HAL_UART_Transmit_IT(&huart1, "-=MUSIC PAUSED=-\r", 17);
			} else if (i == 7 && compareStrings(resume, data, 6) == 1) {

				programState = Resume;
				HAL_UART_Transmit_IT(&huart1, "-=MUSIC RESUME=-\n", 17);

			} else if (i == 12 + digit_count(getDictSize(playlist))
					&& compareStrings(setMusic, data, 10) == 1) {
				extractNumber(data);

			} else if (((i > 13 && i < 17) && data[len - 2] == ')')
					&& compareStrings(setVolume, data, 11) == 1) {
				extractVolume(data);
			} else if ((i == 19) && compareStrings(setShuffle, data, 18) == 1) {
				char message[100] = "[INFO][Play mode changed to Shuffle][";
				int message_len = strlen(message);
				char *time_str = get_current_time();
				for (int i = 0; i < 5; i++) {
					message[message_len++] = time_str[i];
				}
				message[message_len++] = ']';
				message[message_len++] = '\n';
				message[message_len] = '\0';

				HAL_UART_Transmit(&huart1, message, message_len, 100);
				change_program_mode(1);
			} else if ((i == 19) && compareStrings(setLiner, data, 18) == 1) {
				char message[100] = "[INFO][Play mode changed to Ordered][";
				int message_len = strlen(message);
				char *time_str = get_current_time();
				for (int i = 0; i < 5; i++) {
					message[message_len++] = time_str[i];
				}
				message[message_len++] = ']';
				message[message_len++] = '\n';
				message[message_len] = '\0';

				HAL_UART_Transmit(&huart1, message, message_len, 100);
				change_program_mode(0);
			} else {
				HAL_UART_Transmit_IT(&huart1, "INVALID INPUT\n", 14);
			}

			i = 0;
		}

	}
}

//UART END

//ADC Begin
enum ADC_FUNCTION {
	CHANGE_MUSIC, CHANGE_VOLUME, NONE
};
enum ADC_FUNCTION adc_function = NONE;

uint32_t normalize_adc(uint32_t adc_value, uint32_t max_adc_value,
		uint32_t playlist_size) {
	if (adc_function == CHANGE_MUSIC) {
		// Calculate the step size
		float step = (float) max_adc_value / (playlist_size);
		// Calculate the normalized music number
		uint32_t normalized_number = (uint32_t) ((float) adc_value / step + 0.5); // Adding 0.5 for rounding
		// Ensure the normalized number is at least 1
		if (normalized_number < 1) {
			normalized_number = 1;
		}
		adc_select = normalized_number;
		return normalized_number;
	} else if (adc_function == CHANGE_VOLUME) {
		adc_value = (adc_value * 100U) / (max_adc_value - 1);
		if (adc_value > 95) {
			adc_value = 101;
		}
		if (adc_value <= 1)
			adc_value = 1;
		adc_select = adc_value - 1;
		adc_value -= 1;
		if (adc_value <= 0)
			adc_value = 0;
		return adc_value - 1;
	}

}

int adc_indx = 0;
uint32_t adc_values[10];
int denoise_adc() {
	int sum = 0;
	for (int i = 0; i < 10; i++) {
		sum += adc_values[i];
	}
	return sum / 10;
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc) {
	if (hadc->Instance == ADC1) {
		uint32_t value;
		value = HAL_ADC_GetValue(hadc);
		if (adc_indx < 10)
			adc_values[adc_indx++] = value;
		else {
			adc_indx = 0;
			value = denoise_adc();
			extract_int_to_carrier(
					normalize_adc(value, 4095, getDictSize(playlist)));
		}
	}
}
//ADC End

void display_number(int led_flag, int _number) {
	HAL_GPIO_WritePin(GPIOD,
	GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4, GPIO_PIN_SET);
	HAL_GPIO_WritePin(GPIOD, led[led_flag], DISPLAY_ON);
	if (_number != 0) {
		HAL_GPIO_WritePin(GPIOD, digits[_number].pattern, GPIO_PIN_SET);
	}
	HAL_GPIO_WritePin(GPIOD, digits[_number].anti_pattern, GPIO_PIN_RESET);
}

//test removing
void init_display() {
//Reset All Segment Values
	HAL_GPIO_WritePin(GPIOD,
	GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15 | GPIO_PIN_12, GPIO_PIN_SET);
	HAL_GPIO_WritePin(GPIOD,
	GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4, GPIO_PIN_SET);
}

//Carrier is to be displayed on seven segment
int musicNumberSize = 0;
int digit_count(int val) {
	if (val == 0)
		return 1;
	int i, size = 0;
	int temp = val;

	while (temp > 0) {
		temp /= 10;
		size++;
	}
	return size;
}

void extract_int_to_carrier(int val) {
	int i;
	i = digit_count(val) - 1;
	musicNumberSize = digit_count(val);

	while (val > 0) {
		carrier[i--] = val % 10;
		val /= 10;
	}
}

int array_to_number(int *array, int size) {
	int number = 0;
	for (int i = 0; i < size; i++) {
		number = number * 10 + array[i];
	}
	return number;
}

int* number_to_array(int num) {
	if (num == 0) {
		int arr[1] = { 0 };
		return arr;
	}

	int *arr = (int*) malloc(digit_count(num) * sizeof(int));

	int temp = num;
	int i = digit_count(num) - 1;

	while (temp != 0) {
		arr[i--] = temp % 10;
		temp /= 10;
	}

	return arr;
}

int generate_random_int() {
	return random_number = rand() % getDictSize(playlist);
}

void change_program_mode(int shuffle) {
	if (!shuffle) {
		//To resume after last shuffle music
		playedCount = currentMusic;
		programMode = Liner;
	} else {
		programMode = Shuffle;
	}
}

int next_shuffle() {
	struct DictionaryNode *node = NULL;
	int toneCount = 0;
	srand(HAL_GetTick());
	int i = generate_random_int();
	melody = lookup(playlist, playlistOrder[i], &toneCount, &node);

	while (node && isBlacklisted(node)) {
		i = generate_random_int();
		melody = lookup(playlist, playlistOrder[i], &toneCount, &node);
	}
	currentMusic = i + 1;
	if (node) {
		setBlacklisted(node);
		playedCount++;
	}

	return toneCount;
}

//Choose next music based on programMode
void next_music() {
	int toneCount;
	struct DictionaryNode *node;
	if (programState == Resume) {
		if (playedCount >= getDictSize(playlist)) {
			for (int i = 0; i < getDictSize(playlist); i++) {
				melody = lookup(playlist, playlistOrder[i], &toneCount, &node);
				unsetBlacklisted(node);
			}
			playedCount = 0;
		}

		if (programMode == Liner) {
			currentMusic = playedCount + 1;
			melody = lookup(playlist, playlistOrder[playedCount++], &toneCount,
					&node);
			Change_Melody(melody, toneCount);
		} else {
			toneCount = next_shuffle();
			Change_Melody(melody, toneCount);
		}
		extract_int_to_carrier(currentMusic);
	}
}

//Set music directly
void set_music(int num) {
	int toneCount;
	struct DictionaryNode *node;
	melody = lookup(playlist, playlistOrder[num - 1], &toneCount, &node);
	Change_Melody(melody, toneCount);
	currentMusic = num;
	playedCount = num;
	unsetBlacklisted(node);
	extract_int_to_carrier(currentMusic);
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
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

//initialize Global digits structure
	for (int i = 0; i < 10; i++) {
		digits[i] = _digits[i];
	}

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

//Set 7-segment PINS
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
  MX_TIM2_Init();
  MX_USART1_UART_Init();
  MX_ADC1_Init();
  /* USER CODE BEGIN 2 */
	HAL_TIM_Base_Start_IT(&htim1);
	HAL_UART_Transmit_IT(&huart1,
			"=============\nProgram Running\n=============\n", 44);
	HAL_UART_Receive_IT(&huart1, &d, 1);
	initTonesDictionary();
	PWM_Start();


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
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL6;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USB|RCC_PERIPHCLK_USART1
                              |RCC_PERIPHCLK_I2C1|RCC_PERIPHCLK_TIM1
                              |RCC_PERIPHCLK_ADC12;
  PeriphClkInit.Usart1ClockSelection = RCC_USART1CLKSOURCE_PCLK2;
  PeriphClkInit.Adc12ClockSelection = RCC_ADC12PLLCLK_DIV1;
  PeriphClkInit.I2c1ClockSelection = RCC_I2C1CLKSOURCE_HSI;
  PeriphClkInit.USBClockSelection = RCC_USBCLKSOURCE_PLL;
  PeriphClkInit.Tim1ClockSelection = RCC_TIM1CLK_HCLK;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
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
  hadc1.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV1;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc1.Init.ContinuousConvMode = ENABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc1.Init.LowPowerAutoWait = DISABLE;
  hadc1.Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;
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
  sConfig.Channel = ADC_CHANNEL_5;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
  sConfig.SamplingTime = ADC_SAMPLETIME_181CYCLES_5;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset = 0;
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
  hi2c1.Init.Timing = 0x2000090E;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
  {
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
static void MX_SPI1_Init(void)
{

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
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
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
static void MX_TIM1_Init(void)
{

  /* USER CODE BEGIN TIM1_Init 0 */

  /* USER CODE END TIM1_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM1_Init 1 */

  /* USER CODE END TIM1_Init 1 */
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 4800-1;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 5-1;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterOutputTrigger2 = TIM_TRGO2_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
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
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 0;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 4294967295;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
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
  huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
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
static void MX_USB_PCD_Init(void)
{

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
  if (HAL_PCD_Init(&hpcd_USB_FS) != HAL_OK)
  {
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
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

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
  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15
                          |GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3|GPIO_PIN_4, GPIO_PIN_RESET);

  /*Configure GPIO pin : CS_I2C_SPI_Pin */
  GPIO_InitStruct.Pin = CS_I2C_SPI_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(CS_I2C_SPI_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : MEMS_INT4_Pin */
  GPIO_InitStruct.Pin = MEMS_INT4_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_EVT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(MEMS_INT4_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : PA0 */
  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PA1 PA2 PA3 PA4 */
  GPIO_InitStruct.Pin = GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3|GPIO_PIN_4;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PD12 PD13 PD14 PD15
                           PD1 PD2 PD3 PD4 */
  GPIO_InitStruct.Pin = GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15
                          |GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3|GPIO_PIN_4;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI0_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI0_IRQn);

  HAL_NVIC_SetPriority(EXTI1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI1_IRQn);

  HAL_NVIC_SetPriority(EXTI2_TSC_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI2_TSC_IRQn);

  HAL_NVIC_SetPriority(EXTI3_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI3_IRQn);

  HAL_NVIC_SetPriority(EXTI4_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI4_IRQn);

}

/* USER CODE BEGIN 4 */

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
	currentMillis = HAL_GetTick();
	if ((GPIO_Pin == GPIO_PIN_1)) {

		if ((currentMillis - previousMillis > DEBOUNCE_DELAY)) {
//			counterInside++;
			next_music();
			previousMillis = currentMillis;
		}
	} else if (GPIO_Pin == GPIO_PIN_2) {
		if (!HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_2)) {
			if (currentMillis - previousMillis > DEBOUNCE_DELAY
					&& adc_function != CHANGE_MUSIC) {
				// Rising edge of button 2
				adc_function = CHANGE_VOLUME;
				HAL_ADC_Start_IT(&hadc1);
				previousMillis = currentMillis;
			}
		} else {
			// Falling edge of button 2
			adc_function = NONE;
			volume = adc_select;
			char message[100] = "[INFO][Volume changed to ";
			int *helper = number_to_array(adc_select);
			sendInfo(message, helper, adc_select, digit_count(adc_select));
			HAL_ADC_Stop_IT(&hadc1);
			extract_int_to_carrier(currentMusic);
		}

	} else if ((GPIO_Pin == GPIO_PIN_3)) {

		if ((currentMillis - previousMillis > DEBOUNCE_DELAY)) {
			if (programState == Paused || programState == IDLE)
				programState = Resume;
			else if (programState == Resume)
				programState = Paused;
			previousMillis = currentMillis;
		}
	} else if ((GPIO_Pin == GPIO_PIN_4)) {
		if (!HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_4)) {
			if ((currentMillis - previousMillis > DEBOUNCE_DELAY
					&& adc_function != CHANGE_VOLUME)) {
				// Rising edge of button 2
				adc_function = CHANGE_MUSIC;
				HAL_ADC_Start_IT(&hadc1);
				previousMillis = currentMillis;
			}
		} else {
			char message[100] = "[INFO][Music changed to ";
			int helper = number_to_array(adc_select);
			sendInfo(message, helper, adc_select, digit_count(adc_select));
			adc_function = NONE;
			HAL_ADC_Stop_IT(&hadc1);
			set_music(adc_select);
		}
	} else if (GPIO_Pin == GPIO_PIN_0) {
	}
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
	if (htim->Instance == TIM1) {
		timePassed = HAL_GetTick();
		if (index < musicNumberSize) {
			display_number(index, carrier[index]);
		}
		if (index == 4) {
			index = 0;
			init_display();
		} else {
			++index;
		}

	}
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
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
