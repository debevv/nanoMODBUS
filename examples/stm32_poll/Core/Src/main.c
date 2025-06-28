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
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#define NMBS_DEBUG 1
#include "..\..\..\..\nanomodbus.h"
#ifdef NMBS_DEBUG
#include <errno.h>
#include <sys/unistd.h> // STDOUT_FILENO, STDERR_FILENO
#endif


/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

extern UART_HandleTypeDef huart2;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */


#define RTU_SERVER_ADDRESS 84

//invert the red LED
#define RED_TOGGLE()    HAL_GPIO_TogglePin(LED1_SYS_AL_GPIO_Port, LED1_SYS_AL_Pin);

#define SetRS485Receive() HAL_GPIO_WritePin(USART2_RTS_GPIO_Port, USART2_RTS_Pin, GPIO_PIN_RESET)
#define SetRS485Transmit() HAL_GPIO_WritePin(USART2_RTS_GPIO_Port, USART2_RTS_Pin, GPIO_PIN_SET)
#define ToggleRS485Transmit() HAL_GPIO_TogglePin(USART2_RTS_GPIO_Port, USART2_RTS_Pin)

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
uint16_t Speed = 9600;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

int32_t read_serial(uint8_t *buf, uint16_t count, int32_t byte_timeout_ms,
		void *arg) {
	uint16_t received = 0;
	//printf("rs count:%d time:%d ", count,byte_timeout_ms);
	do {
		switch (HAL_UART_Receive(&huart2, buf, 1, byte_timeout_ms)) {
		case HAL_TIMEOUT:
			return received;
			break;
		case HAL_ERROR:
		case HAL_BUSY:
		default:
			return -1;
			break;
		case HAL_OK:
			//printf("%ld %02x\n",HAL_GetTick(),*buf);
			buf++;
			received++;

		}
	} while (received < count);
	return count;
}

int32_t write_serial(const uint8_t *buf, uint16_t count,
		int32_t byte_timeout_ms, void *arg) {
	SetRS485Transmit();
	if (HAL_UART_Transmit(&huart2, buf, count, byte_timeout_ms * count)
			!= HAL_OK) {
		SetRS485Receive();
		return -1;
	}
	SetRS485Receive();
	return (count);
}

void onError(nmbs_error err) {
	printf("error: %d\n", err);
	exit(0);
}
uint16_t my_crc_calc(const uint8_t *data, uint32_t length, void *arg) {
	uint16_t crc = 0xFFFF;
	for (uint32_t i = 0; i < length; i++) {
		crc ^= (uint16_t) data[i];
		for (int j = 8; j != 0; j--) {
			if ((crc & 0x0001) != 0) {
				crc >>= 1;
				crc ^= 0xA001;
			} else
				crc >>= 1;
		}
	}
	return (uint16_t) (crc << 8) | (uint16_t) (crc >> 8);
}

#ifdef NMBS_DEBUG
uint8_t debug_buffer[4096];
uint16_t pos_debug = 0;

int _write(int file, char *data, int len) {
	if ((file != STDOUT_FILENO) && (file != STDERR_FILENO)) {
		errno = EBADF;
		return -1;
	}
	//HAL_UART_Transmit(&huart1, (uint8_t*) data, (uint16_t) len, 0xFFFF);
	//HAL_UART_Transmit_IT(&huart1, (uint8_t*) data, (uint16_t) len);
	strncpy(&debug_buffer[pos_debug], data, len);
	pos_debug += len;
	return len;
}
void flush_debug() {
	HAL_UART_Transmit_IT(&huart1, debug_buffer, pos_debug);
	pos_debug = 0;
}
#endif

/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void) {

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
	MX_USART2_UART_Init();
	MX_USART1_UART_Init();
	/* USER CODE BEGIN 2 */
	uint16_t reg = 110; //number holding register

#ifdef NMBS_DEBUG
	printf("Begin read holding register %d\n", reg);
	printf("Address: %d\n", RTU_SERVER_ADDRESS);
	printf("Speed:%d\n", Speed);
#endif
	nmbs_platform_conf platform_conf;
	nmbs_platform_conf_create(&platform_conf);
	platform_conf.transport = NMBS_TRANSPORT_RTU;
	platform_conf.read = read_serial;
	platform_conf.write = write_serial;
	platform_conf.crc_calc = my_crc_calc;

	nmbs_t nmbs;
	nmbs_error err = nmbs_client_create(&nmbs, &platform_conf);
	if (err != NMBS_ERROR_NONE)
		onError(err);

	nmbs_set_read_timeout(&nmbs, 1000);
	//count timeout 3.5 for modbus = 12 bits*3.5
	nmbs_set_byte_timeout(&nmbs, 12 * 3.5 * 1000 / Speed + 1);

	nmbs_set_destination_rtu_address(&nmbs, RTU_SERVER_ADDRESS);
#ifdef NMBS_DEBUG
	flush_debug();
	HAL_Delay(500);
#endif
	/* USER CODE END 2 */

	/* Infinite loop */
	/* USER CODE BEGIN WHILE */
	while (1) {
		/* USER CODE END WHILE */

		/* USER CODE BEGIN 3 */
		RED_TOGGLE();

		uint16_t r_regs[2];
		err = nmbs_read_holding_registers(&nmbs, reg, 1, r_regs);
		if (err != NMBS_ERROR_NONE) {
			onError(err);
		}
#ifdef NMBS_DEBUG
		printf("register %d is set to: %d\n", reg, r_regs[0]);
		flush_debug();
#endif
		HAL_Delay(500);

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
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
	RCC_OscInitStruct.HSEState = RCC_HSE_ON;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
	RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL6;
	RCC_OscInitStruct.PLL.PREDIV = RCC_PREDIV_DIV1;
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
		Error_Handler();
	}

	/** Initializes the CPU, AHB and APB buses clocks
	 */
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
			| RCC_CLOCKTYPE_PCLK1;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK) {
		Error_Handler();
	}
	PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART1;
	PeriphClkInit.Usart1ClockSelection = RCC_USART1CLKSOURCE_PCLK1;
	if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK) {
		Error_Handler();
	}
}

/* USER CODE BEGIN 4 */

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
void assert_failed(uint8_t *file, uint32_t line) {
	/* USER CODE BEGIN 6 */
	/* User can add his own implementation to report the file name and line number,
	 ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
	/* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
