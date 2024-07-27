## Setup

```
~/build$ export PICO_SDK_PATH=/home/your-username/src/pico-sdk
~/pico-examples/build$ cmake ..
~/pico-examples/build$ make -j8
```

### Check prints

```
~$ minicom -b 115200 -o -D /dev/ttyACM0
```
