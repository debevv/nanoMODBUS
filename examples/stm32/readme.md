# STM32 nanomodbus porting 

## Target hardware

![Blackpill board image](https://dfimg.dfrobot.com/enshop/image/data/DFR0864/Pinout-Diagram.png)

- Blackpill board
- STM32F401CCUx 
- USART1 with DMA
  - PA9 : TX1
  - PA10 : RX1

## Toolchain and environment

Tested on Mac OS Sonoma(Apple Silicon M1) but other os having same toolchain should have no problem.

- STM32CubeMX 6.11.1
- arm-none-eabi-gcc 
- cmake
- ninja
- openocd
- vscode
  - CMake
  - cortex-debug

## How to run

1. Generate driver code from stm32f401ccux.ioc using STM32CubeMX
2. Configure with CMake
3. Launch with elf file or flash it
