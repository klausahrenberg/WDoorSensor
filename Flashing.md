## Notice
Modifying and flashing of devices is at your own risk. I'm not responsible for bricked or damaged devices. I strongly recommend a backup of original firmware before installing any other software.  

## How to flash
I was not able to flash the firmware via tuya-convert. So I did the wired way.


## 1. Prepare your device
* Remove the screw in the battery holder
* There are 4 pinouts, for flashing only use the 2 in the middle, RX and TX
* Connections VCC, GND and GPIO0 have to be soldered directly to the module
* Don't insert the batteries during flashing, then the module can't be flashed

![Flashing connection](https://github.com/klausahrenberg/WDoorSensor/blob/master/docs/images/flash_wiring.jpg)

## 2. Connections for flashing
Following connections were working for me (refer to ESP-12E pinout):
- Red: ESP-VCC connected to Programmer-VCC (3.3V) 
- Black: ESP-GND connected to Programmer-GND
- Black: ESP-GPIO0, must be connected with GND during power up
- Green: ESP-RX connected to Programmer-TX
- Yellow: ESP-TX connected to Programmer-RX

## 3. Backup the original firmware
Don't skip this. In case of malfunction you need the original firmware. Tasmota has also a great tutorial for the right esptool commands: https://github.com/arendst/Sonoff-Tasmota/wiki/Esptool. So the backup command is:

```esptool.py -p <yourCOMport> -b 460800 read_flash 0x00000 0x100000 originalFirmware1M.bin```

for example:

```esptool.py -p /dev/ttyUSB0 -b 460800 read_flash 0x00000 0x100000 originalFirmware1M.bin```

## 4. Upload new firmware
Get the ESP in programming mode first.
Erase flash:

```esptool.py -p /dev/ttyUSB0 erase_flash```

After erasing the flash, get the ESP in programming mode again (GPIO tp GND). 
Write firmware (1MB)

```esptool.py -p /dev/ttyUSB0 write_flash -fs 1MB 0x0 WThermostat_x.xx.bin```

## 5. Prepare for first time run
* Unplug all connections from programmer
* Insert batteries