# stm32 nanomodbus irq(Interrupt request) non blocking mode

simple example how use IRQ USART mode in HAL
##  hardware

![pinout](hardware.png)

- STM32F030
- USART2 with DMA
    - PA3 : RX
    - PA2 : TX
    - PA1 : DE pin for RS-485 (Driver Enable flow control)
## Building

```
make
```

https://github.com/kpvnnov

