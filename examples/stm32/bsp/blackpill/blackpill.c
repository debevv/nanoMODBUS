/*
 * STM32F401CCU6 Board Support Package (BSP) Summary:
 *
 * 1. System Clock Configuration:
 *    - External high-speed oscillator (HSE) enabled.
 *    - PLL is configured with source from HSE, PLLM = 25, PLLN = 336, PLLP = 4,
 * PLLQ = 7.
 *    - System clock (SYSCLK) sourced from PLL output at 84 MHz.
 *    - AHB clock (HCLK) running at SYSCLK.
 *    - APB1 clock (PCLK1) running at HCLK / 2 (42 MHz).
 *    - APB2 clock (PCLK2) running at HCLK.
 *
 * 2. GPIO Configuration:
 *    - GPIOC Pin 13: Configured as output (Push Pull), used for LED control,
 * low frequency.
 *    - GPIOB Pin 7: Configured as input, no pull-up/pull-down, used as input
 * for interrupts.
 *    - GPIOB Pin 6: Configured as open-drain output, low frequency, initially
 * set high.
 *    - GPIOA Pin 15: Configured as output (Push Pull), used for NSS in SPI1
 * communication, very high frequency.
 *    - GPIOA Pins 9 (TX), 10 (RX): Configured as alternate function (AF7) for
 * USART1 communication.
 *    - GPIOB Pins 3 (SCLK), 4 (MISO), 5 (MOSI): Configured as alternate
 * function (AF5) for SPI1 communication.
 *
 * 3. SPI1 Configuration:
 *    - Mode: Master.
 *    - Data Direction: 2-line unidirectional.
 *    - Data Size: 8-bit.
 *    - Clock Polarity: Low when idle.
 *    - Clock Phase: First edge capture.
 *    - NSS (Chip Select): Software management.
 *    - Baud Rate Prescaler: 2.
 *    - First Bit: MSB.
 *    - TI Mode: Disabled.
 *    - CRC Calculation: Disabled.
 *    - Pins: PB3 (SCLK), PB4 (MISO), PB5 (MOSI) configured as alternate
 * function.
 *
 * 4. USART1 Configuration:
 *    - Baud Rate: 115200.
 *    - Word Length: 8 bits.
 *    - Stop Bits: 1.
 *    - Parity: None.
 *    - Mode: TX/RX.
 *    - Hardware Flow Control: None.
 *    - Oversampling: 16x.
 *    - Pins: PA9 (TX), PA10 (RX) configured as alternate function.
 *
 * 5. DMA Configuration:
 *    - DMA2_Stream3 (SPI1_TX): Used for SPI1 TX, configured for
 * memory-to-peripheral, channel 3.
 *      - Memory increment enabled, peripheral increment disabled, normal mode,
 * low priority.
 *      - Linked to SPI1_TX using __HAL_LINKDMA.
 *      - Interrupt priority level 0, enabled.
 *    - DMA2_Stream0 (SPI1_RX): Used for SPI1 RX, configured for
 * peripheral-to-memory, channel 3.
 *      - Memory increment enabled, peripheral increment disabled, normal mode,
 * high priority.
 *      - Linked to SPI1_RX using __HAL_LINKDMA.
 *      - Interrupt priority level 0, enabled.
 *    - DMA2_Stream7 (USART1_TX): Used for USART1 TX, configured for
 * memory-to-peripheral, channel 4.
 *      - Memory increment enabled, peripheral increment disabled, normal mode,
 * low priority.
 *      - Linked to USART1_TX using __HAL_LINKDMA.
 *      - Interrupt priority level 0, enabled.
 *    - DMA2_Stream2 (USART1_RX): Used for USART1 RX, configured for
 * peripheral-to-memory, channel 4.
 *      - Memory increment enabled, peripheral increment disabled, normal mode,
 * high priority.
 *      - Linked to USART1_RX using __HAL_LINKDMA.
 *      - Interrupt priority level 0, enabled.
 *
 * 6. Peripheral Clocks:
 *    - GPIOC, GPIOB, GPIOA clocks enabled for GPIO configuration.
 *    - USART1 clock enabled for UART communication.
 *    - SPI1 clock enabled for SPI communication.
 *    - DMA2 clock enabled for DMA streams (used for SPI1 and USART1).
 *
 * 7. Interrupt Configuration:
 *    - DMA2_Stream3 (SPI1_TX), DMA2_Stream0 (SPI1_RX), DMA2_Stream7
 * (USART1_TX), DMA2_Stream2 (USART1_RX).
 *    - All configured with priority level 0 and interrupts enabled.
 *
 * 8. Error Handling:
 *    - Error_Handler function enters an infinite loop to indicate an error
 * state.
 */

#include "blackpill/blackpill.h"
#include "blackpill.h"

#include "stm32f4xx_hal.h"

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);
static void MX_DMA_Init(void);
static void MX_USART1_UART_Init(void);

SPI_HandleTypeDef hspi1;
DMA_HandleTypeDef hdma_spi1_tx;
DMA_HandleTypeDef hdma_spi1_rx;
UART_HandleTypeDef huart1;
DMA_HandleTypeDef hdma_usart1_tx;
DMA_HandleTypeDef hdma_usart1_rx;

void BSP_Init(void) {
    // Initialize the HAL Library
    HAL_Init();

    // Configure the system clock
    SystemClock_Config();

    // Initialize all configured peripherals (GPIO and SPI1)
    MX_GPIO_Init();
    MX_SPI1_Init();
    MX_DMA_Init();
    MX_USART1_UART_Init();
}

void SystemClock_Config(void) {
    // System Clock Configuration Code
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    // Configure the main internal regulator output voltage
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 25;
    RCC_OscInitStruct.PLL.PLLN = 336;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
    RCC_OscInitStruct.PLL.PLLQ = 7;
    HAL_RCC_OscConfig(&RCC_OscInitStruct);

    // Initialize the CPU, AHB, and APB buses clocks
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2);
}

static void MX_GPIO_Init(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    // Enable GPIOC clock
    __HAL_RCC_GPIOC_CLK_ENABLE();

    // Configure GPIOC Pin 13 for LED output (Output Push Pull mode)
    GPIO_InitStruct.Pin = GPIO_PIN_13;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    // Enable GPIOB clock
    __HAL_RCC_GPIOB_CLK_ENABLE();

    // Configure GPIOB Pin 7 as input for interrupt (Input mode)
    GPIO_InitStruct.Pin = GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    // Configure GPIOB Pin 6 as output with Open Drain mode
    GPIO_InitStruct.Pin = GPIO_PIN_6;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);

    // Enable GPIOA clock
    __HAL_RCC_GPIOA_CLK_ENABLE();
    // Configure NSS pin (PA15) as Output Push Pull
    GPIO_InitStruct.Pin = GPIO_PIN_15;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

static void MX_USART1_UART_Init(void) {
    // USART1 initialization settings
    __HAL_RCC_USART1_CLK_ENABLE();

    huart1.Instance = USART1;
    huart1.Init.BaudRate = 115200;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;

    if (HAL_UART_Init(&huart1) != HAL_OK) {
        // Initialization error handling
        Error_Handler();
    }

    // Enable USART1 interrupt
    // It must higher or equal than 5
    HAL_NVIC_SetPriority(USART1_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);

    // USART1 Pin configuration: TX (PA9), RX (PA10)
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    // Enable GPIOA clock
    __HAL_RCC_GPIOA_CLK_ENABLE();

    // Configure USART1 TX (PA9) and RX (PA10) pins as Alternate Function Push
    // Pull
    GPIO_InitStruct.Pin = GPIO_PIN_9 | GPIO_PIN_10;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

static void MX_SPI1_Init(void) {
    __HAL_RCC_SPI1_CLK_ENABLE();
    // SPI1 initialization settings
    hspi1.Instance = SPI1;
    hspi1.Init.Mode = SPI_MODE_MASTER;                         // Set SPI1 as master
    hspi1.Init.Direction = SPI_DIRECTION_2LINES;               // Set bidirectional data mode
    hspi1.Init.DataSize = SPI_DATASIZE_8BIT;                   // Set data frame size to 8 bits
    hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;                 // Clock polarity low when idle
    hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;                     // First clock transition is the first data capture edge
    hspi1.Init.NSS = SPI_NSS_SOFT;                             // Hardware chip select management
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;    // Set baud rate prescaler to 2
    hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;                    // Data is transmitted MSB first
    hspi1.Init.TIMode = SPI_TIMODE_DISABLE;                    // Disable TI mode
    hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;    // Disable CRC calculation
    hspi1.Init.CRCPolynomial = 10;                             // CRC polynomial value

    if (HAL_SPI_Init(&hspi1) != HAL_OK) {
        // Initialization error handling
        Error_Handler();
    }

    // SPI1 Pin configuration: SCLK (PB3), MISO (PB4), MOSI (PB5)
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    // Enable GPIOB clock
    __HAL_RCC_GPIOB_CLK_ENABLE();

    // Configure SPI1 SCLK, MISO, MOSI pins as Alternate Function Push Pull
    GPIO_InitStruct.Pin = GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF5_SPI1;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

static void MX_DMA_Init(void) {
    // DMA controller clock enable
    __HAL_RCC_DMA2_CLK_ENABLE();

    // Configure DMA request hdma_spi1_tx on DMA2_Stream3
    hdma_spi1_tx.Instance = DMA2_Stream3;
    hdma_spi1_tx.Init.Channel = DMA_CHANNEL_3;
    hdma_spi1_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
    hdma_spi1_tx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_spi1_tx.Init.MemInc = DMA_MINC_ENABLE;
    hdma_spi1_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_spi1_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdma_spi1_tx.Init.Mode = DMA_NORMAL;
    hdma_spi1_tx.Init.Priority = DMA_PRIORITY_LOW;
    hdma_spi1_tx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    if (HAL_DMA_Init(&hdma_spi1_tx) != HAL_OK) {
        // Initialization error handling
        Error_Handler();
    }

    __HAL_LINKDMA(&hspi1, hdmatx, hdma_spi1_tx);

    // Configure DMA request hdma_spi1_rx on DMA2_Stream0
    hdma_spi1_rx.Instance = DMA2_Stream0;
    hdma_spi1_rx.Init.Channel = DMA_CHANNEL_3;
    hdma_spi1_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_spi1_rx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_spi1_rx.Init.MemInc = DMA_MINC_ENABLE;
    hdma_spi1_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_spi1_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdma_spi1_rx.Init.Mode = DMA_NORMAL;
    hdma_spi1_rx.Init.Priority = DMA_PRIORITY_HIGH;
    hdma_spi1_rx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    if (HAL_DMA_Init(&hdma_spi1_rx) != HAL_OK) {
        // Initialization error handling
        Error_Handler();
    }

    __HAL_LINKDMA(&hspi1, hdmarx, hdma_spi1_rx);

    // Configure DMA request hdma_usart1_tx on DMA2_Stream7
    hdma_usart1_tx.Instance = DMA2_Stream7;
    hdma_usart1_tx.Init.Channel = DMA_CHANNEL_4;
    hdma_usart1_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
    hdma_usart1_tx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_usart1_tx.Init.MemInc = DMA_MINC_ENABLE;
    hdma_usart1_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_usart1_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdma_usart1_tx.Init.Mode = DMA_NORMAL;
    hdma_usart1_tx.Init.Priority = DMA_PRIORITY_LOW;
    hdma_usart1_tx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    if (HAL_DMA_Init(&hdma_usart1_tx) != HAL_OK) {
        // Initialization error handling
        Error_Handler();
    }

    __HAL_LINKDMA(&huart1, hdmatx, hdma_usart1_tx);

    // Configure DMA request hdma_usart1_rx on DMA2_Stream2
    hdma_usart1_rx.Instance = DMA2_Stream2;
    hdma_usart1_rx.Init.Channel = DMA_CHANNEL_4;
    hdma_usart1_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_usart1_rx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_usart1_rx.Init.MemInc = DMA_MINC_ENABLE;
    hdma_usart1_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_usart1_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdma_usart1_rx.Init.Mode = DMA_NORMAL;
    hdma_usart1_rx.Init.Priority = DMA_PRIORITY_HIGH;
    hdma_usart1_rx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;

    if (HAL_DMA_Init(&hdma_usart1_rx) != HAL_OK) {
        // Initialization error handling
        Error_Handler();
    }

    __HAL_LINKDMA(&huart1, hdmarx, hdma_usart1_rx);

    // DMA2_Stream3 (SPI1_TX) Interrupt Configuration
    HAL_NVIC_SetPriority(DMA2_Stream3_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(DMA2_Stream3_IRQn);

    // DMA2_Stream0 (SPI1_RX) Interrupt Configuration
    HAL_NVIC_SetPriority(DMA2_Stream0_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);

    // DMA2_Stream7 (USART1_TX) Interrupt Configuration
    HAL_NVIC_SetPriority(DMA2_Stream7_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(DMA2_Stream7_IRQn);

    // DMA2_Stream2 (USART1_RX) Interrupt Configuration
    HAL_NVIC_SetPriority(DMA2_Stream2_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(DMA2_Stream2_IRQn);
}

void Error_Handler(void) {
    // If an error occurs, stay in infinite loop
    while (1) {}
}

void DMA2_Stream3_IRQHandler(void) {
    HAL_DMA_IRQHandler(&hdma_spi1_tx);
}

void DMA2_Stream0_IRQHandler(void) {
    HAL_DMA_IRQHandler(&hdma_spi1_rx);
}

void DMA2_Stream7_IRQHandler(void) {
    HAL_DMA_IRQHandler(&hdma_usart1_tx);
}

void DMA2_Stream2_IRQHandler(void) {
    HAL_DMA_IRQHandler(&hdma_usart1_rx);
}

void USART1_IRQHandler(void) {
    HAL_UART_IRQHandler(&huart1);
}
