# WDoorSensor

[![GitHub version](https://img.shields.io/github/release/klausahrenberg/WDoorSensor.svg)](https://github.com/klausahrenberg/WDoorSensor/releases/latest)

Replaces original Tuya firmware on door sensors with battery and ESP8266 wifi module. The firmware is tested with following device:
* TYMC-1 Earykong (oval shaped door sensor)

The sensor can be purchased on AliExpress for example

![Flashing connection](https://github.com/klausahrenberg/WDoorSensor/blob/master/docs/images/doorsensor.jpg)

## Features
* No Tuya cloud connection anymore
* Enables sensor to send door and battery state via MQTT 
* Configuration of connection and device parameters via web interface
## General way of working with this firmware
* If sensor is activated by door opening/closing (by magnet), the device will connect to MQTT broker and sends the state once. Delay is around 4-6 seconds
* For configuration, hold the reset button until the LED blinks. Then the sensor stays on for around 60 seconds. If no settings found (e.g. after first flash), it will create a Wifi AcessPoint and you can configure it via ```http://192.168.4.1/config```. Otherwise it will connect to the existing Wifi, web interface can be called for configuration via ```http://<your_IP>/config```.
## Installation from original Tuya firmware
To install the firmware, follow instructions here:  
https://github.com/klausahrenberg/WDoorSensor/blob/master/Flashing.md
## Initial configuration
After installation/flashing of firmware, disconnect all connections from programmer and insert the batteries:
* Hold reset button for 5 sec until red LED starts Flashing
* Now you have 60 seconds to configure... If you need more time, move the magnet during configuration.
* Look for wifi AP 'DoorSensor...', Password '12345678'
* Configure Wifi-Settings under ```http://192.168.4.1```
## Upgrade between different version of this firmware
After the installation of this firmware, any newer version can be installed over the air:
* Hold the reset button until the LED blinks. The sensor now stays on for around 60 seconds and you can access the web inteface via ```http://<your_IP>/config```. 
* Go to 'Update firmware' and select the new firmware file, then start the update. 
## Json structure
```json
{
  "idx":"doorsensor",
  "ip":"192.168.x.x",
  "firmware":"x.xx",
  "open":true|false,
  "battery":"low|medium|high"
}
```
### Build this firmware from source
For build from sources you can use the Arduino-IDE, Atom IDE or other. All sources needed are inside the folder 'WDoorSensor' and my other library: https://github.com/klausahrenberg/WAdapter. Additionally you will need some other libraries: DNSServer, EEPROM (for esp8266), ESP8266HTTPClient, ESP8266mDNS, ESP8266WebServer, ESP8266WiFi, Hash, NTPClient, Time - It's all available via board and library manager inside of ArduinoIDE
