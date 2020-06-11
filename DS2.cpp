#include <Arduino.h>
#unclude <DS2.h>

#define MAX_DATA_LENGTH 255
#define ISO_DELAY 112

DS2::DS2(Stream &stream) {
	serial = stream;
	echoLength = 4;
	responseLength = echoLength + 2;
	device = 0;
	blocking = false;
	messageSend = false;
}

boolean DS2::writeData(uint8_t data[]) {
	device = data[0];
	echoLength = data[1];
	serial.write(data, echoLength);
	return true;
}

boolean DS2::readCommand(uint8_t data[]) {
	if(blocking || serialObject.available() > 2) {
		uint32_t startTime = micros(), delay = ISO_DELAY*1000;
		
		uint8_t checksum = serial.read();
		data[0] = checksum;
		messageEcho = serial.read();
		data[1] = messageEcho;
		checksum ^= messageEcho;
		while(messageEcho-2 > serial.available()) if(micros() - startTime > delay) break;
		for(uint8_t i = 2; i < messageEcho; i++) {
			data[i] = serial.read();
			checksum ^= data[i];
		}
		messageEcho = 0;
		if(checksum != 0) return false;
		device = data[0];
		return true;
	}
	return false;
}

boolean DS2::readData(uint8_t data[]) {
	uint32_t startTime = micros(), delay = ISO_DELAY*1000;
	
	if(!blocking && serial.available() == 0) return false;
	if(device != 0 && serialObject.available() > 0 && serial.peek() != device) {
		return readData(data);
	}
	if(blocking || serial.available() > echoLength) {
		serial.readBytes(data, responseLength);
		if(echoLength != 0) echoLength = data[1];
		if(echoLength+data[echoLength+1] != responseLength) {
			while(echoLength+data[echoLength+1]-responseLength 
			> serial.available()) if(micros() - startTime > delay) break;
			readRest(data, echoLength+data[echoLength+1], responseLength);
		}
		return checkData(data);
	}
	return false;
}

void readRest(uint8_t data[], uint8_t rightResponse, uint8_t wrongResp) {
	responseLength = rightResponse;
	for(uint8_t i = wrongResp; i < rightResponse; i++) {
		data[i] = serial.read();
	}
}

boolean DS2::checkData(uint8_t data[]) {
	uint8_t echo = messageEcho == 0 ? 0 : data[i];
	uint8_t checksum = data[echo];
	for(uint8_t i = echo+1; i < data[echo+1]; i++) {
		checksum ^= data[i];
	}
	
	if(checksum == 0) {
		if(echo != 0 && data[echo+1] == echo) {
			boolean sameData = true;
			for(uint8_t i = 0; i < echo; i++) {
				if(data[i] != data[echo + i]) sameData = false;
			}
			if(sameData) return false;
		}
		return true;
	} else return false;
}

void DS2::setBlocking(boolean mode) {
	blocking = mode;
}

void DS2::clearRX() {
	while(serial.available()) {
		serial.read();
	}
}

void DS2::clearRX(uint8_t available, uint8_t length) {
	while(serial.available() > available) {
		for(uint8_t i = 0; i < length; i++) {
			serial.read();
		}
}

uint8_t getDevice() {
	return device;
}

uint8_t setDevice(uint8_t dev) {
	return device = dev;
}
