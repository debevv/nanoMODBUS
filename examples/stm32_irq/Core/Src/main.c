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
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "..\..\..\..\nanomodbus.h"
#ifdef NMBS_DEBUG
#include <errno.h>
#include <sys/unistd.h> // STDOUT_FILENO, STDERR_FILENO
#include <stdio.h>
#define NMBS_DEBUG_DUMP(BUF,LEN) printf(BUF, LEN)
#else
#define NMBS_DEBUG_DUMP(...) (void) (0)
#endif

//#include <stdbool.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

//invert the red LED
#define RED_TOGGLE()    HAL_GPIO_TogglePin(LED1_SYS_AL_GPIO_Port, LED1_SYS_AL_Pin);

#define SetRS485Receive() HAL_GPIO_WritePin(USART2_RTS_GPIO_Port, USART2_RTS_Pin, GPIO_PIN_RESET)
#define SetRS485Transmit() HAL_GPIO_WritePin(USART2_RTS_GPIO_Port, USART2_RTS_Pin, GPIO_PIN_SET)
#define ToggleRS485Transmit() HAL_GPIO_TogglePin(USART2_RTS_GPIO_Port, USART2_RTS_Pin)

// The data model of this sever will support coils addresses 0 to 100 and registers addresses from 0 to 32
#define COILS_ADDR_MAX 100
#define REGS_ADDR_MAX 32

// Our RTU address
#define RTU_SERVER_ADDRESS 1

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
uint16_t Speed = 9600;
nmbs_t nmbs;
volatile uint8_t packet_sended = 0;

// A single nmbs_bitfield variable can keep 2000 coils
nmbs_bitfield server_coils = { 0 };
uint16_t server_registers[REGS_ADDR_MAX + 1] = { 0 };

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

// установка счётчика принятых байтов
void msg_buf_set(nmbs_t *nmbs, uint32_t length) {
	nmbs->msg.buf_rec = length;
}
// чтение счётчика принятых байтов
inline uint32_t msg_buf_get(nmbs_t *nmbs) {
	return nmbs->msg.buf_rec;
}
// увеличение счётчика принятых байтов
inline bool msg_buf_inc(nmbs_t *nmbs) {
	if ((sizeof(nmbs->msg.buf) - nmbs->msg.buf_rec - 1) <= 0)
		return false;
	nmbs->msg.buf_rec++;
	return true;
}

// сброс счётчика принятых байтов в буфере
void msg_rec_reset(nmbs_t *nmbs) {
	nmbs->msg.buf_rec = 0;
}

int32_t read_from_buf(uint8_t *buf, uint16_t count, int32_t byte_timeout_ms,
		void *arg) {

//buf_rec сколько DMA положило байтов в очередь nmbs
//buf_idx это счётчик уже взятых протоколом байтов, и он автоматически инкрементируется,
//поэтому в буфер ничего складывать/переносить не надо - всё и так лежит на своём месте
//процедура симулянт
	return ((nmbs.msg.buf_rec - nmbs.msg.buf_idx - count) >= 0 ?
			count : (nmbs.msg.buf_rec - nmbs.msg.buf_idx));
}

int32_t write_serial(const uint8_t *buf, uint16_t count,
		int32_t byte_timeout_ms, void *arg) {
	SetRS485Transmit();
	if (HAL_UART_AbortReceive(&huart2) != HAL_OK) {
		Error_Handler();
	}
	packet_sended = 1; //устанавливаем флаг отправки пакета, чтобы при modbus poll сразу настроить на вход
	if (HAL_UART_Transmit_IT(&huart2, buf, count) != HAL_OK) {
		NMBS_DEBUG_PRINT("HAL_UART_Transmit_IT error\n");
		// @todo об ошибках надо сообщать индикацией и отказом дальнейшей работы
		//ошибка, конечно, фатальная, но не надо пока зависать в этом месте
		//Error_Handler();
	}
	return count;
}

void nano_RecieveMode(void) {
	SetRS485Receive();
	MX_TIM6_Init();
	msg_rec_reset(&nmbs);

//Receive an amount of data in DMA mode till either the expected number of data is received or an IDLE event occurs
	if (HAL_UART_Receive_IT(&huart2, nmbs.msg.buf, 1) != HAL_OK) {
		NMBS_DEBUG_PRINT("HAL_UART_Receive_IT error\n");

		// @todo об ошибках надо сообщать индикацией и отказом дальнейшей работы
		//ошибка, конечно, фатальная, но не надо пока зависать в этом месте
		//Error_Handler();
	}
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart) {

	if (huart == &huart2) {

		nano_RecieveMode();
	}

}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
	if (huart == &huart2) {
		//restart 3.5 timer
		if (HAL_TIM_Base_Init(&htim6) != HAL_OK) {
			Error_Handler();
		}

		MX_TIM6_Init(); //run 3.5 timer after
		if (msg_buf_inc(&nmbs)) {
			//Receive next symbol
			if (HAL_UART_Receive_IT(&huart2, &nmbs.msg.buf[nmbs.msg.buf_rec], 1)
					!= HAL_OK) {
				NMBS_DEBUG_PRINT("HAL_UART_Receive_IT error\n");

				// @todo об ошибках надо сообщать индикацией и отказом дальнейшей работы
				//ошибка, конечно, фатальная, но не надо пока зависать в этом месте
				//Error_Handler();
			}
		} else { //overflow input buffer
			nano_RecieveMode();
		}
	}
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
	if (htim->Instance == TIM6) {
		uint32_t Size = msg_buf_get(&nmbs);
		if (Size) { //received symbol
			//stop and deinit timer 3.5 word
			if (HAL_TIM_Base_DeInit(&htim6) != HAL_OK) {
				Error_Handler();
			}
			HAL_UART_AbortReceive(&huart2);
			nmbs_error res_poll = nmbs_server_poll(&nmbs);
			if (NMBS_ERROR_NONE != res_poll) {
				NMBS_DEBUG_PRINT(
						"nmbs_server_poll error:%d size of receive:%d\n",
						(int8_t ) res_poll, Size);
				NMBS_DEBUG_PRINT("%s\n", nmbs_strerror(res_poll));
				NMBS_DEBUG_DUMP((uint8_t* )&nmbs, (uint16_t )Size);

				//HAL_UART_Abort(&huart2);
				nano_RecieveMode();
			} else {
				if (!packet_sended) //если в процессе nmbs_server_poll режим не переводили на передачу
					nano_RecieveMode();
			}

		}
	}
}

nmbs_error handle_read_coils(uint16_t address, uint16_t quantity,
		nmbs_bitfield coils_out, uint8_t unit_id, void *arg) {
	if (address + quantity > COILS_ADDR_MAX + 1)
		return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS;

	// Read our coils values into coils_out
	for (int i = 0; i < quantity; i++) {
		bool value = nmbs_bitfield_read(server_coils, address + i);
		nmbs_bitfield_write(coils_out, i, value);
	}

	return NMBS_ERROR_NONE;
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
	//MX_TIM6_Init();
	MX_USART1_UART_Init();
	/* USER CODE BEGIN 2 */

#ifdef NMBS_DEBUG
	printf("Begin\n");
#endif
	nmbs_platform_conf platform_conf;
	nmbs_platform_conf_create(&platform_conf);
	platform_conf.transport = NMBS_TRANSPORT_RTU;
	platform_conf.read = read_from_buf;
	platform_conf.write = write_serial;

	nmbs_callbacks callbacks;
	nmbs_callbacks_create(&callbacks);
	callbacks.read_coils = handle_read_coils;

	nmbs_error err = nmbs_server_create(&nmbs, RTU_SERVER_ADDRESS,
			&platform_conf, &callbacks);

	if (err != NMBS_ERROR_NONE)
		onError(err);

	/* USER CODE END 2 */

	/* Infinite loop */
	/* USER CODE BEGIN WHILE */
	while (1) {
		/* USER CODE END WHILE */

		/* USER CODE BEGIN 3 */
#ifdef NMBS_DEBUG
		flush_debug();
		HAL_Delay(500);
#endif

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
