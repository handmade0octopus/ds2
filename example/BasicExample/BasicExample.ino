#include "DS2.h"

// Go to libraries and paste libraries folder from this example folder
// You can use also Adafruit library although its slower but it supports more screens - code is 100% compatible with it though!
#include "SPI.h"
#include "SPIFFS.h"
#include "TFT_eSPI.h"
TFT_eSPI tft = TFT_eSPI();

// We keep data there, 255 is reccomended for full compatibility, you can use void setMaxDataLength(uint8_t dataLength) if bugs happen
uint8_t data[255];

// Format for commands is always same, see DS2.h for more info
uint8_t ecuId[] = {0x12, 0x04, 0x00, 0x16};
uint8_t generalValues[] = {0x12, 0x05, 0x0B, 0x03, 0x1F};

// Change to true if you want to calibrate LCD again
#define REPEAT_CAL false
#define CALIBRATION_FILE "/TouchCalData1"

// We store our ECU list and byte offset for our battery voltage
struct Ecu {
	char* ecuName;
	uint8_t batteryOffset;
};

uint8_t batteryOffset;

#define ECUS_NUMBER 9

const struct Ecu ecuList[ECUS_NUMBER] PROGMEM = {
	{"default", 16},
	{"1429764", 20},
	{"1430844", 20},
	{"7526753", 20},
	{"7500255", 20},
	{"7511570", 22},
	{"7519308", 22},
	{"7545150", 22},
	{"7551615", 22},
};

// Simple function to match ECU to what we read
uint8_t matchEcu(char* id) {
	Ecu ecu;
	for(uint8_t i = 0; i < ECUS_NUMBER; i++) {
		 memcpy_P(&ecu, &ecuList[i], sizeof ecu);
		 if(compareString(id, ecu.ecuName)) {
			return ecu.batteryOffset; 
		 }
	}
	memcpy_P(&ecu, &ecuList[0], sizeof ecu);
	return ecu.batteryOffset;
}

boolean compareString(char a[], char b[]) {
	boolean match = true;
	for(uint8_t i = 0; i < 255; i++) {
		if(a[i] == 0 || b[i] == 0) break;
		if(a[i] != b[i]) {
			match = false;
			break;
		}
	}
	return match;
}

// We setup our DS2 - ignore ESP32 configuration for now I use it to make sure library will work with both
#if defined ESP32_CUSTOM
		#define TFT_TOUCH_PIN 33
		DS2 DS2(Serial2);
	#else
		#define TFT_TOUCH_PIN 7
		DS2 DS2(Serial);
#endif


void setup() {
	// Setup TFT
	tft.begin();
	tft.setRotation(1);	// landscape
	tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
	tft.fillScreen(ILI9341_BLACK);
	
	// Set up for Serial - ESP32 will be added in future
	#if defined ESP32_CUSTOM
		pinMode(12, OUTPUT);
		digitalWrite(12, HIGH);
//		tft.setRotation(3);	// landscape inverted
		
		Serial2.begin(9600, SERIAL_8E1);
		Serial2.setTimeout(ISO_TIMEOUT);
		while(!Serial2);
		
	#else
		// Setup Serial
		Serial.begin(9600, SERIAL_8E1);
		Serial.setTimeout(ISO_TIMEOUT);
		while(!Serial);
	#endif
	
	pinMode(TFT_TOUCH_PIN, INPUT);
	
	touchCalibrate();
	
	// You can set blocking if you want to test how it works.
//	DS2.setBlocking(true);


	// Loading and displaying ECU ID
	char loading[][3] = {"|", "/", "-", "\\"};
	tft.println(F("CONNECTING"));
	for(uint8_t i = 0; !DS2.obtainValues(ecuId, data); i++) {
		if(i > 3) {
			i = 0;
		}
		tft.setCursor(0, 10);
		tft.print(loading[i]);
	}
	tft.setCursor(0, 0);
	tft.println(F("              "));
	
	// Displaying Ecu ID
	char ecuIdString[8];
	DS2.getString(data, ecuIdString, 0, 7);
	tft.setCursor(0, 0);
	tft.print(ecuIdString);
	
	// Matching battery voltage offset depending on our ECU
	batteryOffset = matchEcu(ecuIdString);
}

// Loop variables
uint32_t startTime;
float fps, lowestFps = 0, highestFps = 0;
boolean print = false;

void loop(void) {
	startTime = micros();
	
	// You can compare performance using two types of commands
	
	// Send command
	if(DS2.sendCommand(generalValues) != 0) // do stuff if data sent
	
	/**
	* You can put some code in between while waiting for data for higher performance
	**/
	if(print) printData(generalValues);

	
	// Receive command
	if((DS2.receiveData(data)) == RECEIVE_OK) print = true; // do stuff if data received
	
	// Blocked .obtainValues - generally slower but easier to use and always laids some response
//	if(DS2.obtainValues(generalValues, data)) print = true;
	
	printFps();
}

// Prints data, gets batteryVoltage from data, you need to specify which command was sent
void printData(uint8_t command[]) {
	tft.setCursor(0,9);
	float batteryVoltage = 0.1*DS2.getByte(data, batteryOffset);
	tft.print(batteryVoltage);
	tft.println(F(" V  "));
	
//	Takes a lot of performance due to how .print is managed. Those screens doesn't like reprints
	
	/*
	printMessage(command, DS2.getEcho());
	tft.setCursor(0,20);
	printMessage(data, DS2.getResponseLength());
	*/
	

	printRps(DS2.getRespondsPerSecond());
	print = false;
}

// Very slow way of printing message, good for debugging though
void printMessage(uint8_t array[], uint8_t length) {
	for(uint8_t i = 0; i < 255; i++) {
		if(i < length) tft.print((array[i]), HEX);
		tft.print(F(" "));
	}
}

// Prints reesponses per second
void printRps(float rps) {
	tft.setCursor(0, 200);
	tft.print(rps);
	tft.println(F(" rps  "));
}

// Prints Fps on the screen with low, high and current. If touch pressed min/max fps are resetted
void printFps() {
	if(digitalRead(TFT_TOUCH_PIN) == 0) {
		lowestFps = 0;
		highestFps = 0;
	}
	tft.setCursor(0, 210);
	fps = 1000000.0/(micros() - startTime);
	if(lowestFps == 0 || lowestFps > fps) lowestFps = fps;
	if(highestFps == 0 || highestFps < fps) highestFps = fps;
	tft.print(lowestFps);
	tft.println(F(" fps Low    "));
	tft.print(fps);
	tft.println(F(" fps Current     "));
	tft.print(highestFps);
	tft.println(F(" fps High    "));
}


void touchCalibrate() {
	uint16_t calData[5];
	uint8_t calDataOK = 0;

	// check if calibration file exists and size is correct
	if (SPIFFS.exists(CALIBRATION_FILE)) {
		if (REPEAT_CAL)
		{
			// Delete if we want to re-calibrate
			SPIFFS.remove(CALIBRATION_FILE);
		}
		else
		{
			File f = SPIFFS.open(CALIBRATION_FILE, "r");
			if (f) {
				if (f.readBytes((char *)calData, 14) == 14)
					calDataOK = 1;
				f.close();
			}
		}
	}

	if (calDataOK && !REPEAT_CAL) {
		// calibration data valid
		tft.setTouch(calData);
	} else {
		// data not valid so recalibrate
		tft.fillScreen(TFT_BLACK);
		tft.setCursor(20, 0);
		tft.setTextFont(2);
		tft.setTextSize(1);
		tft.setTextColor(TFT_WHITE, TFT_BLACK);

		tft.println("Touch corners as indicated");

		tft.setTextFont(1);
		tft.println();

		if (REPEAT_CAL) {
			tft.setTextColor(TFT_RED, TFT_BLACK);
			tft.println("Set REPEAT_CAL to false to stop this running again!");
		}

		tft.calibrateTouch(calData, TFT_MAGENTA, TFT_BLACK, 15);

		tft.setTextColor(TFT_GREEN, TFT_BLACK);
		tft.println("Calibration complete!");

		// store data
		File f = SPIFFS.open(CALIBRATION_FILE, "w");
		if (f) {
			f.write((const unsigned char *)calData, 14);
			f.close();
		}
		tft.setTextColor(TFT_WHITE, TFT_BLACK);
	}
}
