#ifndef DOOR_SENSOR_MCU_H
#define	DOOR_SENSOR_MCU_H

#include "Arduino.h"
#include <ESP8266WiFi.h>
#include <EEPROM.h>
#include "WDevice.h"

#define COUNT_DEVICE_MODELS 2
#define MODEL_BHT_002_GBLW 0
#define MODEL_BAC_002_ALW 1
#define HEARTBEAT_INTERVAL 10000
#define MINIMUM_INTERVAL 2000
#define STATE_COMPLETE 1

const char* BATTERY_LOW = "low";
const char* BATTERY_MEDIUM = "medium";
const char* BATTERY_HIGH = "high";


const unsigned char COMMAND_START[] = {0x55, 0xAA};

class WDoorSensorDevice: public WDevice {
public:

    WDoorSensorDevice(WNetwork* network)
    	: WDevice(network, "thermostat", "thermostat", DEVICE_TYPE_THERMOSTAT) {
			this->open = new WProperty("open", "Open", BOOLEAN, TYPE_OPEN_PROPERTY);
      this->open->setReadOnly(true);
	    this->addProperty(open);
      this->battery = new WProperty("battery", "Battery", STRING, "");
      this->battery->addEnumString(BATTERY_LOW);
      this->battery->addEnumString(BATTERY_MEDIUM);
      this->battery->addEnumString(BATTERY_HIGH);
      this->battery->setReadOnly(true);
      this->addProperty(battery);
    	this->logMcu = false;
    	this->receivingDataFromMcu = false;
    	lastHeartBeat = lastNotify = 0;
    	resetAll();
      this->configButtonPressed = false;
    }

    bool isDeviceStateComplete() {
        return ((!this->open->isNull()) && (!this->battery->isNull()));
    }

    void loop(unsigned long now) {
      if ((!this->configButtonPressed) && (this->isDeviceStateComplete())) {
        //send confirmation to put ESP in deep sleep again
        //55 AA 00 05 00 01 00 05
        commandTuyaToSerial(0x05, 0);
        delay(100);
        commandTuyaToSerial(0x05, 0);
        delay(100);
      }

    	while (Serial.available() > 0) {
    		receiveIndex++;
    		unsigned char inChar = Serial.read();
    		receivedCommand[receiveIndex] = inChar;
    		if (receiveIndex < 2) {
    			//Check command start
    			if (COMMAND_START[receiveIndex] != receivedCommand[receiveIndex]) {
    				resetAll();
    			}
    		} else if (receiveIndex == 5) {
    			//length information now available
    			commandLength = receivedCommand[4] * 0x100 + receivedCommand[5];
    		} else if ((commandLength > -1)
    				&& (receiveIndex == (6 + commandLength))) {
    			//verify checksum
    			int expChecksum = 0;
    			for (int i = 0; i < receiveIndex; i++) {
    				expChecksum += receivedCommand[i];
    			}
    			expChecksum = expChecksum % 0x100;
    			if (expChecksum == receivedCommand[receiveIndex]) {
            Serial.println(this->getCommandAsString());
    				processSerialCommand();
          }
    			resetAll();
    		}
    	}
    	//Heartbeat
    	//long now = millis();
    	if ((HEARTBEAT_INTERVAL > 0)
    			&& ((lastHeartBeat == 0)
    					|| (now - lastHeartBeat > HEARTBEAT_INTERVAL))) {
    		unsigned char heartBeatCommand[] =
    				{ 0x55, 0xAA, 0x00, 0x00, 0x00, 0x00 };
    		commandCharsToSerial(6, heartBeatCommand);
    		//commandHexStrToSerial("55 aa 00 00 00 00");
    		lastHeartBeat = now;
    	}
    }

    unsigned char* getCommand() {
    	return receivedCommand;
    }

    int getCommandLength() {
    	return commandLength;
    }

    String getCommandAsString() {
    	String result = "";
    	if (commandLength > -1) {
    		for (int i = 0; i < 6 + commandLength; i++) {
    			unsigned char ch = receivedCommand[i];
    			result = result + (ch < 16 ? "0" : "") + String(ch, HEX);// charToHexStr(ch);
    			if (i + 1 < 6 + commandLength) {
    				result = result + " ";
    			}
    		}
    	}
    	return result;
    }

    void commandHexStrToSerial(String command) {
    	command.trim();
    	command.replace(" ", "");
    	command.toLowerCase();
    	int chkSum = 0;
    	if ((command.length() > 1) && (command.length() % 2 == 0)) {
    		for (int i = 0; i < (command.length() / 2); i++) {
    			unsigned char chValue = getIndex(command.charAt(i * 2)) * 0x10
    					+ getIndex(command.charAt(i * 2 + 1));
    			chkSum += chValue;
    			Serial.print((char) chValue);
    		}
    		unsigned char chValue = chkSum % 0x100;
    		Serial.print((char) chValue);
    	}
    }

    void commandCharsToSerial(unsigned int length, unsigned char* command) {
    	int chkSum = 0;
    	if (length > 2) {
    		for (int i = 0; i < length; i++) {
    			unsigned char chValue = command[i];
    			chkSum += chValue;
    			Serial.print((char) chValue);
    		}
    		unsigned char chValue = chkSum % 0x100;
    		Serial.print((char) chValue);
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

    void queryState() {
      if (!this->configButtonPressed) {
        network->notice(F("Query state of MCU..."));
        commandTuyaToSerial(0x01);
      }
    }

    void cancelConfiguration() {
    	unsigned char cancelConfigCommand[] = { 0x55, 0xaa, 0x00, 0x03, 0x00, 0x01, 0x02 };
    	commandCharsToSerial(7, cancelConfigCommand);
    }

    void setLogMcu(bool logMcu) {
    	if (this->logMcu != logMcu) {
    		this->logMcu = logMcu;
    		notifyState();
    	}
    }

protected:

private:
    bool configButtonPressed;
    int receiveIndex;
    int commandLength;
    long lastHeartBeat;
    unsigned char receivedCommand[1024];
    bool logMcu;
    boolean receivingDataFromMcu;
    unsigned long lastNotify;
		WProperty* open;
    WProperty* battery;

    int getIndex(unsigned char c) {
    	const char HEX_DIGITS[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8',
    			'9', 'a', 'b', 'c', 'd', 'e', 'f' };
    	int result = -1;
    	for (int i = 0; i < 16; i++) {
    		if (c == HEX_DIGITS[i]) {
    			result = i;
    			break;
    		}
    	}
    	return result;
    }

    void notifyUnknownCommand() {
      network->error(F("Unknown MCU command: '%s'"), this->getCommandAsString().c_str());
    }

    void processSerialCommand() {
    	if (commandLength > -1) {
    		this->receivingDataFromMcu = true;
        byte length = receivedCommand[5];
        bool knownCommand = false;
        switch (receivedCommand[3]) {
        case 0x01:
          //Response to initilization request commandTuyaToSerial(0x01);
          //55 aa 00 01 00 24 7b 22 70 22 3a 22 68 78 35 7a 74 6c 7a 74 69 6a 34 79 78 78 76 67 22 2c 22 76 22 3a 22 31 2e 30 2e 30 22 7d
          commandTuyaToSerial(0x02, 2);
          knownCommand = true;
          break;
        case 0x02:
          //Basic confirmation 55 aa 00 02 00 00
          if (length == 0) {
            //request device state
            commandTuyaToSerial(0x02, 4);
            knownCommand = true;
          }
          break;
        case 0x03:
          //Button was pressed > 5 sec - red blinking led
          //55 aa 00 03 00 00
          this->configButtonPressed = true;
          network->notice(F("Config button pressed..."));
          knownCommand = true;
          break;
        case 0x04:
    			//Setup initialization request
          //55 aa 00 04 00 01 00
    			network->startWebServer();
          knownCommand = true;
          break;
        case 0x05:
          //55 aa 00 05 00 05 01 01 00 01 00 //0: closed 1: open
          //55 aa 00 05 00 05 03 04 00 01 02 //2: ok 0: low battery
          if (length == 5) {
            if ((receivedCommand[6] == 1) && (receivedCommand[7] == 1)) {
              //door state
              this->open->setBoolean(receivedCommand[10] == 0x01);
              knownCommand = true;
            } else if ((receivedCommand[6] == 3) && (receivedCommand[7] == 4)) {
              //battery state
              battery->setString(battery->getEnumString(receivedCommand[10]));
              knownCommand = true;
            }
          }
          break;
        case 0x07:
          if (length == 0) {
            //Sometimes 55 aa 00 07 00 00, mostly at entering configuration, ignore
            knownCommand = true;
          }
          break;
    		}

    		this->receivingDataFromMcu = false;
        if (!knownCommand) {
          notifyUnknownCommand();
        }
      }
    }

  void notifyState() {
  	lastNotify = 0;
  }

  void resetAll() {
    receiveIndex = -1;
    commandLength = -1;
  }

};


#endif
