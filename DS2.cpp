/*
Copyright 2020 - Made by sorek.uk

Permission is hereby granted, free of charge, to any person obtaining a copy of this software
and associated documentation files (the "Software"), to deal in the Software without restriction,
including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <Arduino.h>
#include <DS2.h>

DS2::DS2(Stream &stream):serial(stream) {
	serial = stream;
}

boolean DS2::obtainValues(uint8_t command[], uint8_t data[], uint8_t respLen) {
	responseLength = respLen;
	return obtainValues(command, data);	
}

boolean DS2::obtainValues(uint8_t command[], uint8_t data[]) {
	boolean block = blocking;
	blocking = true;
	writeData(command);
	boolean result = readData(data);
	blocking = block;
	if(!result) clearRX();
	return (result && checkDataOk(data));
}


uint8_t DS2::sendCommand(uint8_t command[], uint8_t respLen) {
	if(messageSend) {
		return 0;
	} else {
		if(respLen != 0) responseLength = respLen;
		messageSend = true;
		clearRX();
		return writeData(command);
	}
}

uint8_t DS2::sendCommand(uint8_t command[]) {
	return sendCommand(command, 0);
}


uint32_t DS2::receiveData(uint8_t data[]) {
	uint32_t time;
	if(messageSend) {
		if(readData(data)) {
			messageSend = false;
			if(!checkDataOk(data)) {
				return RECEIVE_BAD;
			}
			return RECEIVE_OK;
		} else if((time = (micros() - timeStamp)) < delay) {
			return RECEIVE_WAITING;
		} else {
			messageSend = false;
			return time;
//			return RECEIVE_TIMEOUT;
		}
	} else return RECEIVE_WAITING;
}

uint8_t DS2::writeData(uint8_t data[]) {
	timeStamp = micros();
	device = data[0];
	echoLength = data[1];
	return serial.write(data, echoLength);
}

uint8_t DS2::writeData(uint8_t data[], uint8_t length) {
	timeStamp = micros();
	device = data[0];
	echoLength = data[1];
	responseLength = length;
	return serial.write(data, length);
}

boolean DS2::readCommand(uint8_t data[]) {
	if(blocking || serial.available() > 1) {
			uint32_t startTime = micros();
		
			uint8_t checksum = serial.read();
			data[0] = checksum;
			echoLength = serial.read();
			data[1] = echoLength;
			checksum ^= echoLength;
			while(echoLength-2 > serial.available()) if(micros() - startTime > delay) break;
			for(uint8_t i = 2; i < echoLength; i++) {
				data[i] = serial.read();
				checksum ^= data[i];
			}
			echoLength = 0;
			if(checksum != 0) return false;
			device = data[0];
			return true;
	}
	return false;
}

boolean DS2::readData(uint8_t data[]) {
	uint32_t startTime = micros();
	if(!blocking && echoLength != 0 && serial.available() == 0) return false;
	if(device != 0 && serial.available() > 0 && serial.peek() != device) {
		serial.read();
		return readData(data);
	}
	if(blocking || serial.available() > echoLength) {
		if(echoLength + 2 > responseLength) responseLength = maxDataLength; 
		responseLength = serial.readBytes(data, responseLength);
		uint8_t readRespLength = echoLength+data[echoLength+1];
//		if(data[echoLength+1] == 0) return false;
/*
		if(echoLength != 0 && echoLength != data[1]) {
			clearRX();
			return false;
		}
		*/
		if(readRespLength > responseLength) {
			if(!readRest(data, readRespLength, responseLength, startTime)) return false;
		} 
//		if(echoLength + 2 > readRespLength) return false;
		responseLength = readRespLength;
		return checkData(data);
	}
	return false;
}

void DS2::clearData(uint8_t data[]) {
	for(uint8_t i = 0; i < maxDataLength; i++) {
		data[i] = 0;
	}
}

boolean DS2::readRest(uint8_t data[], uint8_t rightResponseLength, uint8_t wrongResponseLength, uint32_t startTime) {
	responseLength = rightResponseLength;
	while(rightResponseLength-wrongResponseLength > serial.available()) if(micros() - startTime > delay) return false;
	for(uint8_t i = wrongResponseLength; i < responseLength; i++) {
		data[i] = serial.read();
	}
	return true;
}

boolean DS2::checkData(uint8_t data[]) {
	uint8_t echo = echoLength == 0 ? 0 : data[1];
	uint8_t checksum = data[echo];
	for(uint8_t i = echo+1; i < data[echo+1]+echo; i++) {
		checksum ^= data[i];
	}

	if(checksum == 0) {
		if(echo != 0 && echo < maxDataLength/2 && data[echo+1] == echo) {
			boolean sameData = true;
			for(uint8_t i = 0; i < echo; i++) {
				if(data[i] != data[echo + i]) sameData = false;
			}
			if(sameData) return false;
		}
		commandsPerSecond = 1000000.0/(micros() - timeStamp);
		timeStamp = micros();
		return true;
	} else return false;
}

boolean DS2::checkDataOk(uint8_t data[]) {
	if(!ackByteCheck || data[echoLength+ackByteOffset] == ackByte) return true;
	else return false;
}

void DS2::setAckByte(uint8_t ack, uint8_t offset, boolean check) {
	ackByte = ack;
	ackByteOffset = offset;
	ackByteCheck = check;
}

void DS2::setBlocking(boolean mode) {
	blocking = mode;
}

void DS2::setTimeout(uint8_t timeoutMs) {
	isoTimeout = timeoutMs;
	delay = isoTimeout*1000.0;
}

void DS2::clearRX() {
	while(serial.available() > 0) {
		serial.read();
	}
}

void DS2::clearRX(uint8_t available, uint8_t length) {
	while(serial.available() > available) {
		for(uint8_t i = 0; i < length; i++) {
			serial.read();
		}
	}
}

float DS2::getRespondsPerSecond(){
	return commandsPerSecond;
}

uint8_t DS2::getDevice() {
	return device;
}

uint8_t DS2::setDevice(uint8_t dev) {
	return device = dev;
}

uint8_t DS2::getEcho() {
	return echoLength;
}

uint8_t DS2::getResponseLength() {
	return responseLength;
}

void DS2::setMaxDataLength(uint8_t dataLength) {
	maxDataLength = dataLength;
}


// Getting data
uint8_t DS2::getByte(uint8_t data[], uint8_t offset) {
	uint8_t dataPoint = echoLength + offset + 3;
	return data[dataPoint];
}

uint16_t DS2::getInt(uint8_t data[], uint8_t offset){
	uint16_t result = 0;
	uint8_t dataPoint = echoLength + offset + 3;
	((uint8_t *)&result)[0] = data[dataPoint++];
	((uint8_t *)&result)[1] = data[dataPoint];
	return result;
}
	
uint8_t DS2::getString(uint8_t data[], char string[], uint8_t offset, uint8_t length) {
	uint8_t charPos = 0;
	uint8_t totalOffset = offset + echoLength + 3;
	for(uint8_t i = totalOffset; i < length + totalOffset; i++) {
		string[charPos++] = (char) data[i];
		if(i + 1 == length + totalOffset) string[charPos++] = (char) 0;
		if(data[i] == 0) {
			charPos--;
			break;
		}
	}
	return charPos;
}
	
uint8_t DS2::getString(uint8_t data[], char string[], uint8_t offset) {
	return getString(data, string, offset, 255);
}
	