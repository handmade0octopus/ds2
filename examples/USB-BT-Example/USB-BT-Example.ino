#include "DS2.h" 
// ESP32 ONLY!
// Go to libraries and paste libraries folder from this example folder
// You can use also Adafruit library although its slower but it supports more screens - code is 100% compatible with it though!
#include "SPI.h"
#include "TFT_eSPI.h"
TFT_eSPI tft = TFT_eSPI();

// We setup our DS2
#include <BluetoothSerial.h>
BluetoothSerial SerialBT;

#define TFT_TOUCH_PIN 33
DS2 BT(SerialBT);
DS2 USB(Serial);
DS2 DS2(Serial2);


#define UART_SELECT 25 
#define LED_SELECT 12

// We keep data there, 255 is reccomended for full compatibility, you can use void setMaxDataLength(uint8_t dataLength) if bugs happen
uint8_t data[255];
uint8_t btData[127];



void setup() {
	// Setup TFT
	tft.begin();
	tft.setRotation(1);	// landscape
	tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
	tft.fillScreen(ILI9341_BLACK);
	
	// Set up for Serial
	pinMode(LED_SELECT, OUTPUT);
	digitalWrite(LED_SELECT, HIGH);
	// tft.setRotation(3);	// landscape inverted
	
	// Changes pin select so USB communicates directly to k-line
	pinMode(UART_SELECT, OUTPUT);
	digitalWrite(UART_SELECT, HIGH);
	

	Serial2.begin(9600, SERIAL_8E1);
	Serial2.setTimeout(ISO_TIMEOUT);
	
	Serial.begin(9600, SERIAL_8E1);
	Serial.setTimeout(ISO_TIMEOUT);
	
	SerialBT.begin("MS4x ESP32");
	
	
	pinMode(TFT_TOUCH_PIN, INPUT);	
}

// Loop variables
uint32_t startTime;
float fps, lowestFps = 0, highestFps = 0;
bool print = false;
bool received = false;

void loop(void) {
	startTime = micros();


	if(USB.readData(data)) {
		// Do stuff when USB data spotted
		DS2.setDevice(data[0]);
		USB.setDevice(data[0]);
		DS2.setEcho(data[1]);
		tft.setCursor(0, 0);
		tft.println(F("USB"));
		printMessage(data, DS2.getEcho());
	} else if(BT.readCommand(btData)) {
		// Do stuff when BT data spotted	
		if(DS2.available() == 0) {
			digitalWrite(UART_SELECT, LOW); // We take pin low of UART_SELECT so we can send data to ECU
			DS2.writeData(btData);
			received = true;
			while(DS2.available() < DS2.getEcho()) { // There need to be some delay between states on the pin
				if(micros() - startTime > 1000000.0) break;
			}
			digitalWrite(UART_SELECT, HIGH);
		}
		tft.setCursor(0, 0);
		tft.println(F("BT "));
		printMessage(btData, DS2.getEcho());
		
	}
	
	// Receive command
	if((DS2.readData(data)))  {
		// Do stuff if data received
		received = true;
		tft.setCursor(0, 80);
		tft.print(DS2.getEcho());
		tft.println(F("   "));
		tft.print(DS2.getResponseLength());
		tft.println(F("   "));
		printMessage(data, DS2.getResponseLength());
		printRps(DS2.getRespondsPerSecond());
	}
	
	// Speeds up bluetooth reading
	if(received) {
		BT.writeData(data, DS2.getResponseLength());
		received = false;
	}
	
	printFps();
}

// Prints data
void printData(uint8_t command[]) {


	printRps(DS2.getRespondsPerSecond());
	print = false;
}

// Very slow way of printing message, good for debugging though
void printMessage(uint8_t array[], uint8_t length) {
	for(uint8_t i = 0; i < 128; i++) {

		if(i < length) {
			if(array[i] < 0x10) tft.print(0, HEX);
			tft.print((array[i]), HEX);
		} else tft.print(F("  "));
		if(i == 0 || (i+1)%18 != 0) tft.print(F(" "));
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


