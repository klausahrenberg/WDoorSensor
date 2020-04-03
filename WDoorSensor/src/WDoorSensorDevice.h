#ifndef DOOR_SENSOR_MCU_H
#define	DOOR_SENSOR_MCU_H

#include "Arduino.h"
#include <ESP8266WiFi.h>
#include <EEPROM.h>
#include "WDevice.h"

const static char HTTP_CONFIG_CHECKBOX_RELAY[]         PROGMEM = R"=====(
		<div>
			<label>
				<input type="checkbox" name="rs" value="true" %s>Relay at GPIO 5
			</label>
			<br>
			<small>* Hardware modification is needed at Thermostat to make this work.</small>
		</div>
)=====";


#define COUNT_DEVICE_MODELS 2
#define MODEL_BHT_002_GBLW 0
#define MODEL_BAC_002_ALW 1
#define HEARTBEAT_INTERVAL 10000
#define MINIMUM_INTERVAL 2000
#define STATE_COMPLETE 5
#define PIN_STATE_HEATING_RELAY 5
#define PIN_STATE_COOLING_RELAY 4

const unsigned char COMMAND_START[] = {0x55, 0xAA};
const char AR_COMMAND_END = '\n';
const String SCHEDULES = "schedules";
const char* SCHEDULES_MODE_OFF = "off";
const char* SCHEDULES_MODE_AUTO = "auto";
const char* SYSTEM_MODE_NONE = "none";
const char* SYSTEM_MODE_COOL = "cool";
const char* SYSTEM_MODE_HEAT = "heat";
const char* SYSTEM_MODE_FAN = "fan_only";
const char* STATE_OFF = "off";
const char* STATE_HEATING = "heating";
const char* STATE_COOLING = "cooling";
const char* FAN_MODE_NONE = "none";
const char* FAN_MODE_AUTO = "auto";
const char* FAN_MODE_LOW  = "low";
const char* FAN_MODE_MEDIUM  = "medium";
const char* FAN_MODE_HIGH = "high";

const byte STORED_FLAG_BECA = 0x36;
const char SCHEDULES_PERIODS[] = "123456";
const char SCHEDULES_DAYS[] = "wau";

class WDoorSensorDevice: public WDevice {
public:
    typedef std::function<bool()> THandlerFunction;
    typedef std::function<bool(const char*)> TCommandHandlerFunction;

    WDoorSensorDevice(WNetwork* network)
    	: WDevice(network, "thermostat", "thermostat", DEVICE_TYPE_THERMOSTAT) {
    	this->logMcu = false;
    	this->receivingDataFromMcu = false;
			this->providingConfigPage = true;
    	lastHeartBeat = lastNotify = 0;
    	resetAll();
    	for (int i = 0; i < STATE_COMPLETE; i++) {
    		receivedStates[i] = false;
    	}
    }

    virtual void printConfigPage(WStringStream* page) {
    	network->log()->notice(F("Beca thermostat config page"));
    	page->printAndReplace(FPSTR(HTTP_CONFIG_PAGE_BEGIN), getId());

			//page->printAndReplace(FPSTR(HTTP_CHECKBOX_OPTION), "cr", "cr", (this->sendCompleteDeviceState() ? "" : "checked"), "", "Send every property change in a single MQTT message");

    	page->print(FPSTR(HTTP_CONFIG_SAVE_BUTTON));
    }

    void saveConfigPage(ESP8266WebServer* webServer) {
        network->log()->notice(F("Save Sensor config page"));
        //this->thermostatModel->setByte(webServer->arg("tm").toInt());
        //this->schedulesDayOffset->setByte(webServer->arg("ws").toInt());
    }

    void loop(unsigned long now) {
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

    void queryState() {
    	//55 AA 00 08 00 00
    	unsigned char queryStateCommand[] = { 0x55, 0xAA, 0x00, 0x08, 0x00, 0x00 };
    	commandCharsToSerial(6, queryStateCommand);
    }

    void cancelConfiguration() {
    	unsigned char cancelConfigCommand[] = { 0x55, 0xaa, 0x00, 0x03, 0x00, 0x01,
    			0x02 };
    	commandCharsToSerial(7, cancelConfigCommand);
    }



    void setOnNotifyCommand(TCommandHandlerFunction onNotifyCommand) {
    	this->onNotifyCommand = onNotifyCommand;
    }

    void setOnConfigurationRequest(THandlerFunction onConfigurationRequest) {
    	this->onConfigurationRequest = onConfigurationRequest;
    }



    /*void fanModeToMcu(WProperty* property) {
    	if ((fanMode != nullptr) && (!this->receivingDataFromMcu)) {
    		byte dt = this->getFanModeAsByte();
    		if (dt != 0xFF) {
    			//send to device
    		    //auto:   55 aa 00 06 00 05 67 04 00 01 00
    			//low:    55 aa 00 06 00 05 67 04 00 01 03
    			//medium: 55 aa 00 06 00 05 67 04 00 01 02
    			//high:   55 aa 00 06 00 05 67 04 00 01 01
    			unsigned char deviceOnCommand[] = { 0x55, 0xAA, 0x00, 0x06, 0x00, 0x05,
    			                                    0x67, 0x04, 0x00, 0x01, dt};
    			commandCharsToSerial(11, deviceOnCommand);
    		}
    	}
    }*/

    void setLogMcu(bool logMcu) {
    	if (this->logMcu != logMcu) {
    		this->logMcu = logMcu;
    		notifyState();
    	}
    }

    bool isDeviceStateComplete() {
    	if (network->isDebug()) {
    		return true;
    	}
    	for (int i = 0; i < STATE_COMPLETE; i++) {
    		if (receivedStates[i] == false) {
    			return false;
    		}
    	}
    	return true;
    }

protected:

private:
    int receiveIndex;
    int commandLength;
    long lastHeartBeat;
    unsigned char receivedCommand[1024];
    bool logMcu;
    boolean receivingDataFromMcu;
    boolean receivedStates[STATE_COMPLETE];
    THandlerFunction onConfigurationRequest;
    TCommandHandlerFunction onNotifyCommand;
    unsigned long lastNotify;

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

    void notifyMcuCommand(const char* commandType) {
    	if ((logMcu) && (onNotifyCommand)) {
    		onNotifyCommand(commandType);
    	}
    }

    void processSerialCommand() {
    	if (commandLength > -1) {
    		//unknown
    		//55 aa 00 00 00 00
    		this->receivingDataFromMcu = true;

    		if (receivedCommand[3] == 0x00) {
    			switch (receivedCommand[6]) {
    			case 0x00:
    			case 0x01:
    				//ignore, heartbeat MCU
    				//55 aa 01 00 00 01 01
    				//55 aa 01 00 00 01 00
    				break;
    			//default:
    				//notifyUnknownCommand();
    			}
    		} else if (receivedCommand[3] == 0x03) {
    			//ignore, MCU response to wifi state
    			//55 aa 01 03 00 00
    		} else if (receivedCommand[3] == 0x04) {
    			//Setup initialization request
    			//received: 55 aa 01 04 00 00
    			if (onConfigurationRequest) {
    				//send answer: 55 aa 00 03 00 01 00
    				unsigned char configCommand[] = { 0x55, 0xAA, 0x00, 0x03, 0x00,
    						0x01, 0x00 };
    				commandCharsToSerial(7, configCommand);
    				onConfigurationRequest();
    			}

    		} else if (receivedCommand[3] == 0x07) {
    			bool changed = false;
    			bool newB;
    			float newValue;
    			byte newByte;
    			byte commandLength = receivedCommand[5];
    			bool knownCommand = false;
    			//Status report from MCU
    			switch (receivedCommand[6]) {
    			case 0x01:
    				if (commandLength == 0x05) {
    					//device On/Off
    					//55 aa 00 06 00 05 01 01 00 01 00|01
    					newB = (receivedCommand[10] == 0x01);
    					//changed = ((changed) || (newB != deviceOn->getBoolean()));
    					//deviceOn->setBoolean(newB);
    					receivedStates[0] = true;
    					notifyMcuCommand("deviceOn_x01");
    					knownCommand = true;
    				}
    				break;
    			}
    			if (!knownCommand) {
    				notifyUnknownCommand();
    			} else if (changed) {
    				notifyState();    		
    			}

    		} else if (receivedCommand[3] == 0x1C) {
    			//Request for time sync from MCU : 55 aa 01 1c 00 00
    			//this->sendActualTimeToBeca();
    		} else {
    			notifyUnknownCommand();
    		}
    		this->receivingDataFromMcu = false;
    	}
    }

    /*void deviceOnToMcu(WProperty* property) {
    	if (!this->receivingDataFromMcu) {
       		//55 AA 00 06 00 05 01 01 00 01 01
       		byte dt = (this->deviceOn->getBoolean() ? 0x01 : 0x00);
       		unsigned char deviceOnCommand[] = { 0x55, 0xAA, 0x00, 0x06, 0x00, 0x05,
       		                                    0x01, 0x01, 0x00, 0x01, dt};
       		commandCharsToSerial(11, deviceOnCommand);
       		//notifyState();
     	}
    }*/

    void notifyState() {
    	lastNotify = 0;
    }

    void resetAll() {
       	receiveIndex = -1;
       	commandLength = -1;
    }

    void notifyUnknownCommand() {
    	if (onNotifyCommand) {
    		onNotifyCommand("unknown");
    	}
    }

};


#endif
