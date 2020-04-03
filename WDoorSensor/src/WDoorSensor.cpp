#include <Arduino.h>
#include "WNetwork.h"
#include "WDoorSensorDevice.h"

#define APPLICATION "Door Window Sensor"
#define VERSION "1.02"
#define DEBUG false

WNetwork* network;
WDoorSensorDevice* dsDevice;

void setup() {
	Serial.begin(9600);
	//Wifi and Mqtt connection
	network = new WNetwork(DEBUG, APPLICATION, VERSION, true, NO_LED);
	network->setOnNotify([]() {
		if (network->isWifiConnected()) {
			//nothing to do
		}
		if (network->isMqttConnected()) {
			dsDevice->queryState();
			if (dsDevice->isDeviceStateComplete()) {
				//nothing to do;
			}
		}
	});
	network->setOnConfigurationFinished([]() {
		//Switch blinking thermostat in normal operating mode back
		dsDevice->cancelConfiguration();
	});
	//Communication between ESP and MCU
	dsDevice = new WDoorSensorDevice(network);
	network->addDevice(dsDevice);
	//unknown commands
	dsDevice->setOnNotifyCommand([](const char* commandType) {
		return network->publishMqtt("mcucommand", commandType, dsDevice->getCommandAsString().c_str());
	});
	dsDevice->setOnConfigurationRequest([]() {
		network->startWebServer();
		return true;
	});
}

void loop() {
	network->loop(millis());
	delay(50);
}
