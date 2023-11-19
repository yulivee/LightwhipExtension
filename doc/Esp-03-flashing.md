![](FZZQQ3HLDX41CRE.jpg)

The ESP-03 has a 4 Mbit 25Q40BT part which allows for 512 KB of program space. Of that, about 423 KB is available for your own programs.

## Flash Mode 
To flash the ESP-03 connect the following:

![](fritzing-esp03-flash-mode.png)

| ESP PIN    | FTDI Flasher |
| ---------- | ------------ |
| VCC        | VCC          |
| GND        | GND          |
| RX         | TX           |
| TX         | RX           |
| CH_PD / EN | VCC          |
| GPIO 0     | GND          |
| GPIO 15    | GND          |

Before starting the Upload, disconnect the flasher from the ESP. Do not disconnect the USB-Cable! Disconnect the cables on the flasher!

For the flashing to work, the following settings have to be present in the file `platformio.ini`:

```ini
[env:esp03]
platform = espressif8266
board = d1
board_build.flash_mode = dout
board_build.ldscript = eagle.flash.512k64.ld
upload_resetmethod = ck
framework = arduino
monitor_speed = 115200
upload_speed = 115200
```

## Normal Mode

For normal operation of the ESP-03 connect the following:

![](fritzing-esp03-normal-mode.png)

| ESP PIN    | FTDI Flasher    |
| ---------- | --------------- |
| VCC        | VCC             |
| GND        | GND             |
| RX         | TX              |
| TX         | RX              |
| CH_PD / EN | VCC             |
| GPIO 0     | floating or VCC |
| GPIO 15    | GND             |

## Operation

The ESP 03 needs stable 3.3V to work. The voltage delivered by the flasher is not sufficient for wireless operation and will result in very weird connection errors, because the chip can get a brown-out while trying to connect to the wireless LAN. Use an additional 3.3V power supply.