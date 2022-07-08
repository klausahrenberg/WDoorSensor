#include <Arduino.h>
#include "WNetwork.h"
#include "WDoorSensorDevice.h"
#include "WStringStream.h"

#define APPLICATION "Door Window Sensor"
#define VERSION "1.25"
#define FLAG_SETTINGS 0x22
#define DEBUG false

WNetwork* network;
WDoorSensorDevice* dsDevice;

void setup() {
	Serial.begin(9600);
	//Wifi and Mqtt connection
	network = new WNetwork(DEBUG, APPLICATION, VERSION, NO_LED, FLAG_SETTINGS, nullptr);
	network->setSupportingWebThing(false);
	network->setLastWillEnabled(false);
	network->setOnNotify([]() {
		if (network->isWifiConnected()) {
			//nothing to do
		}
		if (network->isMqttConnected()) {
			if (dsDevice->isDeviceStateComplete()) {
				network->debug(F("state complete"));
			} else {
				network->debug(F("state not complete"));
					dsDevice->queryDeviceState();
			}

		}
	});
	network->setOnConfigurationFinished([]() {
		//Switch blinking thermostat in normal operating mode back
		network->debug(F("cancel config"));
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
