#ifndef DOOR_SENSOR_MCU_H
#define	DOOR_SENSOR_MCU_H

#include "Arduino.h"
#include <ESP8266WiFi.h>
#include <EEPROM.h>
#include "WTuyaDevice.h"

#define COUNT_DEVICE_MODELS 2
#define MODEL_BHT_002_GBLW 0
#define MODEL_BAC_002_ALW 1
#define HEARTBEAT_INTERVAL 10000
#define MINIMUM_INTERVAL 2000
#define STATE_COMPLETE 1

const char* BATTERY_LOW = "low";
const char* BATTERY_MEDIUM = "medium";
const char* BATTERY_HIGH = "high";


class WDoorSensorDevice: public WTuyaDevice {
public:

  WDoorSensorDevice(WNetwork* network)
  	: WTuyaDevice(network, "sensor", "sensor", DEVICE_TYPE_DOOR_SENSOR) {
    this->usingCommandQueue = false;
		this->open = new WProperty("open", "Open", BOOLEAN, TYPE_OPEN_PROPERTY);
    this->open->setReadOnly(true);
	  this->addProperty(open);
    this->battery = new WProperty("battery", "Battery", STRING, "");
    this->battery->addEnumString(BATTERY_LOW);
    this->battery->addEnumString(BATTERY_MEDIUM);
    this->battery->addEnumString(BATTERY_HIGH);
    this->battery->setReadOnly(true);
    this->addProperty(battery);
    this->configButtonPressed = false;
    this->notifyAllMcuCommands->setBoolean(false);
    this->wasClosedWithoutMqttConnection = false;
  }

  bool isDeviceStateComplete() {
    return ((!this->open->isNull()) && (!this->battery->isNull()));
  }

  virtual void cancelConfiguration() {
    //send confirmation to put ESP in deep sleep again
    //55 AA 00 05 00 01 00 05
    commandTuyaToSerial(0x05, 0);
    delay(250);
    commandTuyaToSerial(0x05, 0);
    delay(250);
  }

  void loop(unsigned long now) {
    if ((!this->configButtonPressed) && (this->isDeviceStateComplete())) {
      //set the ESP in sleep mode
      cancelConfiguration();
    }
    WTuyaDevice::loop(now);
    if ((wasClosedWithoutMqttConnection) && (network->isInitialMqttSent())) {
      this->open->setBoolean(false);
      wasClosedWithoutMqttConnection = false;
    }
  }

  void commandTuyaToSerial(byte commandByte) {
    commandTuyaToSerial(commandByte, 0xFF);
  }

  void commandTuyaToSerial(byte commandByte, byte value) {
    unsigned char tuyaCommand[] = { 0x55, 0xAA, 0x00, 0x00, 0x00, 0x00, 0x00 };
    tuyaCommand[3] = commandByte;
    tuyaCommand[5] = (value != 0xFF ? 0x01 : 0x00);
    tuyaCommand[6] = (value != 0xFF ? value : 0x00);
    commandCharsToSerial(6 +  (value != 0xFF ? 1 : 0), tuyaCommand);
  }

  void queryDeviceState() {
    if (!this->configButtonPressed) {
      network->debug(F("Query state of MCU..."));
      commandTuyaToSerial(0x01);
    }
  }

 protected:

  virtual bool processCommand(byte commandByte, byte length) {
    bool knownCommand = false;
    switch (commandByte) {
      case 0x01: {
        //Response to initilization request commandTuyaToSerial(0x01);
        //55 aa 00 01 00 24 7b 22 70 22 3a 22 68 78 35 7a 74 6c 7a 74 69 6a 34 79 78 78 76 67 22 2c 22 76 22 3a 22 31 2e 30 2e 30 22 7d
        commandTuyaToSerial(0x02, 2);
        knownCommand = true;
        break;
      }
      case 0x02: {
        //Basic confirmation 55 aa 00 02 00 00
        if (length == 0) {
          //request device state
          commandTuyaToSerial(0x02, 4);
          knownCommand = true;
        }
        break;
      }
      case 0x03: {
        //Button was pressed > 5 sec - red blinking led
        //55 aa 00 03 00 00
        this->configButtonPressed = true;
        network->debug(F("Config button pressed..."));
        knownCommand = true;
        break;
      }
      case 0x04: {
        //Setup initialization request
        //55 aa 00 04 00 01 00
        network->startWebServer();
        knownCommand = true;
        break;
      }
      case 0x05: {
        //55 aa 00 05 00 05 01 01 00 01 00 //0: closed 1: open
        //55 aa 00 05 00 05 03 04 00 01 02 //2: ok 0: low battery
        if (length == 5) {
          if ((receivedCommand[6] == 1) && (receivedCommand[7] == 1)) {
            //door state
            bool isOpen = (receivedCommand[10] == 0x01);
            if ((network->isInitialMqttSent()) || (isOpen) || (this->open->isNull())) {
              this->open->setBoolean(isOpen);
            } else if (!isOpen) {
              wasClosedWithoutMqttConnection = true;
            }
            knownCommand = true;
          } else if ((receivedCommand[6] == 3) && (receivedCommand[7] == 4)) {
            //battery state
            battery->setString(battery->getEnumString(receivedCommand[10]));
            knownCommand = true;
          }
        }
        break;
      }
    }
    return knownCommand;
  }

  virtual bool processStatusCommand(byte statusCommandByte, byte length) {
    //Sometimes 55 aa 00 07 00 00, mostly at entering configuration, ignore
    return (length == 0);
  }

private:
  bool configButtonPressed;
	WProperty* open;
  WProperty* battery;
  bool wasClosedWithoutMqttConnection;

};


#endif
