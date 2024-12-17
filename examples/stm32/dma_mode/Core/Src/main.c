/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 This example application sets up an RTU server and handles modbus requests
 used HAL stm32 in DMA mode

 This server supports the following function codes:
 FC 01 (0x01) Read Coils
 FC 03 (0x03) Read Holding Registers
 FC 15 (0x0F) Write Multiple Coils
 FC 16 (0x10) Write Multiple registers

 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "dma.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "nanomodbus.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#ifdef NMBS_DEBUG
#include <stdio.h>
#define NMBS_DEBUG_PRINT(...) printf(__VA_ARGS__)
#else
#define NMBS_DEBUG_PRINT(...) (void) (0)
#endif

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
void msg_rec_reset(nmbs_t*);
void modbus_receive_DMA(nmbs_t*, UART_HandleTypeDef*);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

// The data model of this sever will support coils addresses 0 to 100 and registers addresses from 0 to 32
#define COILS_ADDR_MAX 100
#define REGS_ADDR_MAX 32

// Our RTU address
#define RTU_SERVER_ADDRESS 1

nmbs_t nmbs;
nmbs_platform_conf platform_conf;
nmbs_callbacks callbacks = { 0 };
nmbs_bitfield_256 server_coils = { 0 };
uint16_t server_registers[REGS_ADDR_MAX] = { 0 };

volatile uint8_t packet_sended = 0;

/**
 * @brief reading from the same buff[] where everything was read via DMA
 * @retval num bytes readed
 */

int32_t read_from_buf(uint8_t *buf, uint16_t count, int32_t byte_timeout_ms,
		void *arg) {

//buf_rec how many DMA put bytes into the nmbs queue
//buf_idx this is a counter of bytes already taken by the protocol
//therefore, there is no need to add/transfer anything to the buf[] - everything is already in its place
//USART monitors timeouts by hardware
	return ((nmbs.msg.buf_rec - nmbs.msg.buf_idx - count) >= 0 ?
			count : (nmbs.msg.buf_rec - nmbs.msg.buf_idx));
}
/**
 * @brief setting the DMA reception mode
 * @retval 
 */

void modbus_receive_DMA(nmbs_t *nmbs_rec, UART_HandleTypeDef *huart) {
	msg_rec_reset(nmbs_rec);
//Receive an amount of data in DMA mode till either the expected number of data is received or an IDLE event occurs
	if (HAL_UARTEx_ReceiveToIdle_DMA(huart, nmbs_rec->msg.buf,
			sizeof(nmbs_rec->msg.buf)) != HAL_OK) //
			{
		NMBS_DEBUG_PRINT("HAL_UARTEx_ReceiveToIdle_DMA error\n");
		//Error_Handler();
	}
}

/**
 * @brief sending data using DMA
 * @retval 
 */
int32_t modbus_send_DMA(const uint8_t *buf, uint16_t count,
		int32_t byte_timeout_ms, void *arg) {
	packet_sended = 1; //set flag sended packet
	if (HAL_UART_Transmit_DMA(&huart2, buf, count) != HAL_OK) {
		NMBS_DEBUG_PRINT("HAL_UART_Transmit_DMA error\n");
	}
	return count;
}

/**
 * @brief reset the counter of received bytes in the buffer
 * @retval 
 */
void msg_rec_reset(nmbs_t *nmbs) {
	nmbs->msg.buf_rec = 0;
}
/**
 * @brief set the counter of received bytes to the buffer using DMA
 * @retval 
 */
void msg_buf_set(nmbs_t *nmbs, uint32_t length) {
	nmbs->msg.buf_rec = length;
}

/**
 * @brief The HAL_UART_TxCpltCallback() user callbacks
 *         will be executed respectively at the end of the transmit process
 *         When this event occurs, the DMA receive mode must be set
 * @retval 
 */

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart) {

	if (huart == &huart2) {
		modbus_receive_DMA(&nmbs, huart);
	}
}

/**

 * @brief  Reception Event Callback (Rx event notification called after use of advanced reception service).
 * @param  huart UART handle
 * @param  Size  Number of data available in application reception buffer (indicates a position in
 *               reception buffer until which, data are available)
 */

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size) {
	if (huart == &huart2) {

		switch (HAL_UARTEx_GetRxEventType(huart)) {
		case HAL_UART_RXEVENT_IDLE: //when Idle event occurred prior reception has been completed (nb of received data is lower than expected one).
			packet_sended = 0; // clear flag of transmit
			msg_buf_set(&nmbs, Size); //set size of received data
			nmbs_error res_poll = nmbs_server_poll(&nmbs);
			if (NMBS_ERROR_NONE != res_poll) {
				// This will probably never happen, since we don't return < 0 in our platform funcs
				NMBS_DEBUG_PRINT(
						"nmbs_server_poll error:%d size of receive:%d\n",
						(int8_t ) res_poll, Size);
				NMBS_DEBUG_PRINT("%s\n", nmbs_strerror(res_poll));
				modbus_receive_DMA(&nmbs, huart);
			} else {
				//If there was no packet transmission, then we immediately switch to receive mode. Otherwise, this function will trigger an interrupt at the end of DMA packet transmission
				if (!packet_sended)
					modbus_receive_DMA(&nmbs, huart);
			}
			break;
		}

	}

}
/**
 * @brief  UART error callbacks
 * @param  UartHandle: UART handle
 * @note   This example shows a simple way to report transfer error, and you can
 *         add your own implementation.
 * @retval None
 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *UartHandle) {
//Error_Handler();
	NMBS_DEBUG_PRINT(
			"HAL_UART_ErrorCallback Some report transfer error RS-485 %d\n",
			(uint16_t )UartHandle->ErrorCode);
	// @todo Extend error handling as you see fit
	modbus_receive_DMA(&nmbs, UartHandle);
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

nmbs_error handle_write_multiple_coils(uint16_t address, uint16_t quantity,
		const nmbs_bitfield coils, uint8_t unit_id, void *arg) {
	if (address + quantity > COILS_ADDR_MAX + 1)
		return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS;

	// Write coils values to our server_coils
	for (int i = 0; i < quantity; i++) {
		nmbs_bitfield_write(server_coils, address + i,
				nmbs_bitfield_read(coils, i));
	}

	return NMBS_ERROR_NONE;
}

nmbs_error handler_read_holding_registers(uint16_t address, uint16_t quantity,
		uint16_t *registers_out, uint8_t unit_id, void *arg) {
	if (address + quantity > REGS_ADDR_MAX + 1)
		return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS;

	// Read our registers values into registers_out
	for (int i = 0; i < quantity; i++)
		registers_out[i] = server_registers[address + i];

	return NMBS_ERROR_NONE;
}

nmbs_error handle_write_multiple_registers(uint16_t address, uint16_t quantity,
		const uint16_t *registers, uint8_t unit_id, void *arg) {
	if (address + quantity > REGS_ADDR_MAX + 1)
		return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS;

	// Write registers values to our server_registers
	for (int i = 0; i < quantity; i++)
		server_registers[address + i] = registers[i];

	return NMBS_ERROR_NONE;
}

void modbus_init() {
    nmbs_platform_conf_create(&platform_conf);
	platform_conf.transport = NMBS_TRANSPORT_RTU;
	platform_conf.read = read_from_buf;
	platform_conf.write = modbus_send_DMA;
	platform_conf.arg = NULL;

	callbacks.read_coils = handle_read_coils;
	callbacks.write_multiple_coils = handle_write_multiple_coils;
	callbacks.read_holding_registers = handler_read_holding_registers;
	callbacks.write_multiple_registers = handle_write_multiple_registers;

	// Create the modbus server
	nmbs_error err = nmbs_server_create(&nmbs, RTU_SERVER_ADDRESS,
			&platform_conf, &callbacks);
	if (err != NMBS_ERROR_NONE) {
		Error_Handler();
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
	/* USER CODE BEGIN 2 */
	modbus_init();
	modbus_receive_DMA(&nmbs, &huart2);

	/* USER CODE END 2 */

	/* Infinite loop */
	/* USER CODE BEGIN WHILE */
	while (1) {
		/* USER CODE END WHILE */
		/* USER CODE BEGIN 3 */
		//Here you can add the logic of the main program. At this point, modbus communication is in interrupt mode and DMA
		RED_TOGGLE();
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
