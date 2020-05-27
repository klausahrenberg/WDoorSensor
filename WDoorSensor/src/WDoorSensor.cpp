#include <Arduino.h>
#include "WNetwork.h"
#include "WDoorSensorDevice.h"
#include "WStringStream.h"

#define APPLICATION "Door Window Sensor"
#define VERSION "1.11"
#define DEBUG false

WNetwork* network;
WDoorSensorDevice* dsDevice;

void setup() {
	Serial.begin(9600);
	//Wifi and Mqtt connection
	network = new WNetwork(DEBUG, APPLICATION, VERSION, false, NO_LED);
	network->setSupportingWebThing(false);
	network->setOnNotify([]() {
		if (network->isWifiConnected()) {
			//nothing to do
		}
		if (network->isMqttConnected()) {
			dsDevice->queryState();
		}
	});
	network->setOnConfigurationFinished([]() {
		//Switch blinking thermostat in normal operating mode back
		dsDevice->cancelConfiguration();
	});
	//Communication between ESP and MCU
	dsDevice = new WDoorSensorDevice(network);
	network->addDevice(dsDevice);
}

void loop() {
	network->loop(millis());
	delay(50);
}
