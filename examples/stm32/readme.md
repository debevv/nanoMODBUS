# STM32 nanomodbus porting

## Target hardware

![Blackpill board image](https://dfimg.dfrobot.com/enshop/image/data/DFR0864/Pinout-Diagram.png)

- Blackpill board
- STM32F401CCUx
- USART1 (with/without) DMA
    - PA9 : TX1
    - PA10 : RX1
- SPI1 with DMA (connected to W5500)
    - PB3 : SCK1
    - PB4 : MISO1
    - PB5 : MOSI1
    - PA15 : NSS (Software select)

## Toolchain and environment

Tested on Mac OS Sonoma(Apple Silicon) & Windows 10 but other os having same toolchain should have no problem.

- arm-none-eabi-gcc
- cmake
- ninja
- openocd
- vscode
    - CMake
    - cortex-debug

## Building

```
mkdir build
cd build
cmake ..
make -j16
```
