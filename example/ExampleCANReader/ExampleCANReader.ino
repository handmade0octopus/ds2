#include "DS2.h"

// Go to libraries and paste libraries folder from this example folder
// You can use also Adafruit library although its slower but it supports more screens - code is 100% compatible with it though!
#include "SPI.h"
#include "TFT_eSPI.h"
TFT_eSPI tft = TFT_eSPI();

#include <ESP32CAN.h>
#include <CAN_config.h>

CAN_device_t CAN_cfg;

// SD card library and pins
#include "SD.h"

#include <BluetoothSerial.h>
BluetoothSerial SerialBT;

// SD car update feature
#include <Update.h>
#define UPDATE_FILE "/update.bin"

#define SD_CS 15
#define SD_SCK 14
#define SD_MISO 27
#define SD_MOSI 13

SPIClass SDSPI(HSPI);

/**
To use connect to serial 115200 baud rate and send command:

S10000,0x1,8,1,0,1,2,3,4,5,6,7,8

Number of repeats, ID, Number of Bytes, Delay (1ms recommended), Payload and other bytes can be both dec or 0x for Hex
*/




// We setup our DS2 - ignore ESP32 configuration for now I use it to make sure library will work with both
#define TFT_TOUCH_PIN 33
DS2 DS2(Serial2);


void setup() {
	// Setup TFT
	tft.begin();
	tft.setRotation(0);	// 1 = landscape
	tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
	tft.fillScreen(ILI9341_BLACK);
	
	// Set up for Serial - ESP32 will be added in future
	pinMode(12, OUTPUT);
	digitalWrite(12, HIGH);
	
	Serial.begin(115200);
	
	SerialBT.begin("CAN Reader");	
		
	Serial2.begin(9600, SERIAL_8E1);
	Serial2.setTimeout(ISO_TIMEOUT);
	while(!Serial2);
	
	SDSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
	if(!SD.begin(SD_CS, SDSPI)) {
		Serial.println("SD fail");
	} else updateFirmware();
	
	pinMode(TFT_TOUCH_PIN, INPUT);
	
	CAN_cfg.speed=CAN_SPEED_500KBPS;
    CAN_cfg.tx_pin_id = GPIO_NUM_5;
    CAN_cfg.rx_pin_id = GPIO_NUM_4;
    /* create a queue for CAN receiving */
    CAN_cfg.rx_queue = xQueueCreate(10, sizeof(CAN_frame_t));
    //initialize CAN Module
    ESP32Can.CANInit();
	
	tft.setCursor(0,0);
	tft.println("CAN");
	
}

// Loop variables
uint32_t startTime;
float fps, lowestFps = 0, highestFps = 0;
float hz, lowestHz = 0, highestHz = 0;
boolean print = false;
uint32_t canTimestamp = 0;
boolean gotCan = false;
CAN_frame_t rx_frame;
CAN_frame_t tx_frame;

#define MAX_CAN_LIST 50
uint16_t canList[MAX_CAN_LIST][10];
uint8_t currentCan = 0;
uint8_t currentMaxCan = 0;

#define MAX_CAN_SEND 50
#define CAN_FUNC 13
uint16_t canSender[MAX_CAN_SEND][CAN_FUNC];
uint8_t currentCanSender = 0;
uint8_t maxCanSender = 0;

void loop(void) {
	startTime = micros();
	
	if(Serial.available()) readSerial(Serial);
	else if(SerialBT.available()) readSerial(SerialBT);
	
	
	if(xQueueReceive(CAN_cfg.rx_queue,&rx_frame, 3*portTICK_PERIOD_MS)==pdTRUE){
		gotCan = true;
		boolean newCan = true;
		for(uint8_t i = 0; i < currentMaxCan && i < MAX_CAN_LIST; i++) {
			if(canList[i][0] == rx_frame.MsgID) {
				newCan = false;
				currentCan = i;
				break;
			}
		}
		if(newCan) canList[currentCan = currentMaxCan++][0] = rx_frame.MsgID;
		
		if(rx_frame.FIR.B.RTR != CAN_RTR) {
			if(rx_frame.FIR.B.FF == CAN_frame_std) {
				canList[currentCan][9] = (uint8_t) rx_frame.FIR.B.DLC;
				for(uint8_t i = 0; i < canList[currentCan][9]; i++){
					canList[currentCan][i+1] = rx_frame.data.u8[i];
				}
			} else {
				canList[currentCan][9] = (uint8_t) rx_frame.FIR.B.DLC*4;
				for(uint8_t i = 0; i < canList[currentCan][9]; i++){
					uint8_t j = i > 3 ? 1 : 0;
					canList[currentCan][i+1] = ((uint8_t *)&rx_frame.data.u32[j])[i - j*4];
				}
			}
		}		
	}
	
	sendCan();
	
	if(gotCan) printCan();
	
	printFps();
}

void readSerial(Stream &ser) {
	if(ser.peek() == 'S') {
		char* end;
		uint16_t newCanSender[CAN_FUNC]; 
		String read = ser.readString();
		String sub = read.substring(1);
		
		for(uint8_t i = 0; i < CAN_FUNC; i++) {
			newCanSender[i] = 0;
		}
		
		for(uint8_t i = 0; i < CAN_FUNC; i++) {
			int subIndex = sub.indexOf(",");
			if(subIndex == -1) {
				newCanSender[i] = (uint16_t) strtoull((char*) sub.c_str(), &end, 0);
				break;
			} else {
				newCanSender[i] = (uint16_t) strtoull((char*) sub.substring(0, subIndex).c_str(), &end, 0);
				sub = sub.substring(subIndex + 1);
			}	
		}
		
		boolean newSender = true;
		for(uint8_t i = 0; i < maxCanSender && i < MAX_CAN_SEND; i++) {
			if(canSender[i][1] == newCanSender[1]) {
				newSender = false;
				currentCanSender = i;
				break;
			}
		}
		if(newSender) currentCanSender = maxCanSender++;
		for(uint8_t i = 0; i < CAN_FUNC; i++) {
			canSender[currentCanSender][i] = newCanSender[i];
		}
		ser.print("Sent: ");
		ser.println(read);
	} else if(ser.peek() == 'C') {
		ESP32Can.CANStop();
		
		char* end;
		String read = ser.readString();
		read = read.substring(1);
		uint16_t canSpeed = (uint16_t) strtoull((char*) read.c_str(), &end, 0);
		
		switch(canSpeed) {
			case 100:
				CAN_cfg.speed=CAN_SPEED_100KBPS;
				break;
			case 125:
				CAN_cfg.speed=CAN_SPEED_125KBPS;
				break;
			case 250:
				CAN_cfg.speed=CAN_SPEED_250KBPS;
				break;
			case 500:
				CAN_cfg.speed=CAN_SPEED_500KBPS;
				break;
			case 800:
				CAN_cfg.speed=CAN_SPEED_800KBPS;
				break;
			default: CAN_cfg.speed=CAN_SPEED_1000KBPS;
		}
		ESP32Can.CANInit();
	} else if(ser.available()) ser.read();
}

void sendCan() {
	for(uint8_t canIterator = 0; canIterator < maxCanSender; canIterator++) {
		if(canSender[canIterator][0]) {
			canSender[canIterator][0]--;
			tx_frame.FIR.B.FF = CAN_frame_std;
			tx_frame.MsgID = canSender[canIterator][1];
			tx_frame.FIR.B.DLC = canSender[canIterator][2];
			for(uint8_t i = 0; i < canSender[canIterator][2]; i++) {
				tx_frame.data.u8[i] = canSender[canIterator][4+i];
			}
			ESP32Can.CANWriteFrame(&tx_frame);
			delay(canSender[canIterator][3]);
		}
	}
}

void printCan() {
	uint16_t offset = 9;
	for(uint8_t	i = 0; i < currentMaxCan; i++) {
		if(canList[currentCan][0] == canList[i][0]) {
			offset += i*9;
			break;
		}
	}
	tft.setCursor(0, offset);
	tft.print("ID: 0x");
	if(canList[currentCan][0] < 0x10) tft.print("0");	
	if(canList[currentCan][0] < 0x100) tft.print("0");	
	tft.print(canList[currentCan][0], HEX);
	tft.print(" Data: ");
	for(uint8_t i = 0; i < canList[currentCan][9]; i++) {
		tft.print(" ");
		if(canList[currentCan][i+1] < 16) tft.print("0");
		tft.print(canList[currentCan][i+1], HEX);
	}
}

void updateFirmware() {
	File updateFile = SD.open(UPDATE_FILE);
	if (updateFile == NULL || updateFile.isDirectory()) {
		Serial.println("Update failed");
		return;
	}
	
	size_t updateSize = updateFile.size();
	
	if(Update.begin(updateSize)) {
		Serial.print("Update Started: ");
		Serial.println(updateSize);
		if(Update.writeStream(updateFile) == updateSize) {
			if(Update.end() && Update.isFinished()) {
				updateFile.close();
				SD.remove(UPDATE_FILE);
				delay(3000);
				ESP.restart();
			}
		}
	}
}


// Prints Fps on the screen with low, high and current. If touch pressed min/max fps are resetted
void printFps() {
	if(digitalRead(TFT_TOUCH_PIN) == 0) {
		lowestFps = 0;
		highestFps = 0;
		lowestHz = 0;
		highestHz = 0;
	}
	
	if(gotCan) {
		hz = 1000000.0/(micros() - canTimestamp);
		gotCan = false;
		canTimestamp = micros();
	}
	fps = 1000000.0/(micros() - startTime);
	if(lowestFps == 0 || lowestFps > fps) lowestFps = fps;
	if(highestFps == 0 || highestFps < fps) highestFps = fps;
	if(lowestHz == 0 || lowestHz > hz) lowestHz = hz;
	if(highestHz == 0 || highestHz < hz) highestHz = hz;
	
	tft.setCursor(0, 270);
	tft.print(lowestHz);
	tft.println(F(" Hz Low    "));
	tft.print(hz);
	tft.println(F(" Hz     "));
	tft.print(highestHz);
	tft.println(F(" Hz High    "));
	
	
//	tft.setCursor(0, 290);
	tft.print(lowestFps);
	tft.println(F(" fps Low    "));
	tft.print(fps);
	tft.println(F(" fps Current     "));
	tft.print(highestFps);
	tft.println(F(" fps High    "));
}


