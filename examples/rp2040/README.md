# Raspberry Pi Pico W (RP2040) RTU client

## Setup

Install pico-sdk from https://github.com/raspberrypi/pico-sdk

``` sh
export PICO_SDK_PATH=/path/to/your/pico/sdk
mkdir -p build && cd build
cmake ..
make -j8
```

## Check prints

``` sh
minicom -b 115200 -o -D /dev/ttyACM0
```
