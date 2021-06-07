#define ESP32_CUSTOM
#include "DS2.h"
// Go to libraries and paste libraries folder from this example folder
// You can use also Adafruit library although its slower but it supports more screens - code is 100% compatible with it though!
#include "SPI.h"
#include "TFT_eSPI.h"
TFT_eSPI tft = TFT_eSPI();

// SD card library and pins
#include "SD.h"

// SD car update feature
#include <Update.h>
#define UPDATE_FILE "/update.bin"

#define SD_CS 15
#define SD_SCK 14
#define SD_MISO 27
#define SD_MOSI 13

SPIClass SDSPI(HSPI);

// We keep data there, 255 is reccomended for full compatibility, you can use void setMaxDataLength(uint8_t dataLength) if bugs happen
uint8_t data[255];


#define TFT_TOUCH_PIN 33
#define UART_SELECT 25
#define TFT_LED_CTRL 12
DS2 DS2(Serial2);

// The scrolling area must be a integral multiple of TEXT_HEIGHT
#define TEXT_HEIGHT 8 // Height of text to be printed and scrolled
#define BOT_FIXED_AREA 0 // Number of lines in bottom fixed area (lines counted from bottom of screen)
#define TOP_FIXED_AREA 32 // Number of lines in top fixed area (lines counted from top of screen)
#define XMAX 240 // Number of lines in top fixed area (lines counted from top of screen)
#define YMAX 320 // Bottom of screen area

// The initial y coordinate of the top of the scrolling area
uint16_t yStart = TOP_FIXED_AREA;
// yArea must be a integral multiple of TEXT_HEIGHT
uint16_t yArea = YMAX-TOP_FIXED_AREA-BOT_FIXED_AREA;
// The initial y coordinate of the top of the bottom text line
uint16_t yDraw = YMAX - BOT_FIXED_AREA - TEXT_HEIGHT;


boolean sdReady = false;
boolean fileReady = false;
boolean toggleLog = false;
void setup() {
	// Setup TFT
	tft.begin();
	tft.setRotation(0);	// landscape
	tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
	tft.fillScreen(ILI9341_BLACK);
	
	// Turn on screen
	pinMode(TFT_LED_CTRL, OUTPUT);
	digitalWrite(TFT_LED_CTRL, HIGH);
	
	// Set serial to pass all data USB <--> K-Line DS2
	pinMode(UART_SELECT, OUTPUT);
	digitalWrite(UART_SELECT, HIGH);
//	tft.setRotation(3);	// landscape inverted

	Serial.begin(9600, SERIAL_8E1);
	Serial.setTimeout(5);
		
	Serial2.begin(9600, SERIAL_8E1);
	Serial2.setTimeout(5);
	while(!Serial2);

	
	SDSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
	
	SDSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
	if(!(sdReady = SD.begin(SD_CS, SDSPI))) {
//		Serial.println("SD fail");
	} else updateFirmware();
	
	pinMode(TFT_TOUCH_PIN, INPUT);
	
//	DS2.setDevice(0x12);
//	DS2.setKwp(true);
	setupScrollArea(TOP_FIXED_AREA, BOT_FIXED_AREA);
}

// Loop variables
uint32_t startTime;
uint8_t respLength;
void loop(void) {
	startTime = micros();
	tft.setCursor(0, 13);
	//if(Serial.peek() == "A0") {
//		Serial.read();

//	}

	if(Serial.available()) {
		while(Serial.available()) {
			tft.print(Serial.read(), HEX);
			tft.print(" ");
		}
		tft.print((micros() - startTime)/1000.0);
	}

	if(Serial2.available()) {
		respLength = Serial2.readBytes(data, 255);
		printMessage(data, respLength);
		if(fileReady) logToFile(data, respLength);
	}

	
	
	
	
	handleSDCard();
	printFps();
}



// Handling SD card event
String path = "/log01.csv";
uint8_t fileNumber = 1;
uint32_t lastcheck;
File file;
boolean handleSDCard() {
	if(!sdReady) {
		if(millis() < lastcheck + 1000L) return false;
		lastcheck = millis();
		if(SD.begin(SD_CS, SDSPI)) sdReady = true;
		else sdReady = false;
	} else {
		sdReady = true;
		if(fileReady) {
//			
			if(toggleLog) {
				file.close();
				fileReady = false;
				toggleLog = false;
			}
		} else if(toggleLog) {
			file = SD.open(path.c_str());
			while(file) {
				String fileNumberName = String(++fileNumber);
				path = path.substring(0, path.indexOf(".csv")-fileNumberName.length());
				path = path + fileNumberName + ".csv";
				file = SD.open(path.c_str());
			}
			if(!(file = SD.open(path.c_str(), FILE_WRITE))) return false;
			file.print("Timestamp,");
			for(uint8_t i = 0; i < 255; i++) {
				file.print("B" + String(i) + ",");
			}
			file.println();
			lastcheck = millis();
			fileReady = true;
			toggleLog = false;
		}
	}
	
	return sdReady && fileReady;
}

void logToFile(uint8_t dat[], uint8_t respLen) {
	String text = String(millis() - lastcheck);
	for(uint8_t i = 0; i < respLen; i++) {
		text += ",";
		if(dat[i] < 0x10) text += String(0, HEX);
		text += String(dat[i], HEX);
	}
	file.println(text);
	file.flush();
}



// Very slow way of printing message, good for debugging though
void printMessage(uint8_t array[], uint8_t length) {
	yDraw = scroll_line();
	tft.setCursor(0, yDraw);
	for(uint8_t i = 0; i < length; i++) {
		if(array[i] < 16) tft.print("0");
		if(i < length) tft.print((array[i]), HEX);
		tft.print(F(" "));
		if(i > 0 && i%12 == 0) {
			yDraw = scroll_line();
			tft.setCursor(0, yDraw);
			tft.print(F("   "));
		}
	}
}


// Prints Fps on the screen with low, high and current. If touch pressed min/max fps are resetted
uint32_t lastEvent;
void printFps() {
	if(digitalRead(TFT_TOUCH_PIN) == 0) {
		if(millis() < lastEvent + 1000L) return; // debounce
		lastEvent = millis();
		toggleLog = true;
	}
	
	tft.setCursor(0, 4);
	tft.print(sdReady ? "SD READY " : "SD FAIL  ");
	tft.print(fileReady ? "LOGGING " : "        ");
	tft.print(path);
}

// ##############################################################################################
// Call this function to scroll the display one text line
// ##############################################################################################
int scroll_line() {
  int yTemp = yStart; // Store the old yStart, this is where we draw the next line

  tft.fillRect(0,yStart,XMAX,TEXT_HEIGHT, TFT_BLACK);

  // Change the top of the scroll area
  yStart+=TEXT_HEIGHT;
  // The value must wrap around as the screen memory is a circular buffer
  if (yStart >= YMAX - BOT_FIXED_AREA) yStart = TOP_FIXED_AREA + (yStart - YMAX + BOT_FIXED_AREA);
  // Now we can scroll the display
  scrollAddress(yStart);
  return  yTemp;
}

// ##############################################################################################
// Setup a portion of the screen for vertical scrolling
// ##############################################################################################
// We are using a hardware feature of the display, so we can only scroll in portrait orientation
void setupScrollArea(uint16_t tfa, uint16_t bfa) {
  tft.writecommand(ILI9341_VSCRDEF); // Vertical scroll definition
  tft.writedata(tfa >> 8);           // Top Fixed Area line count
  tft.writedata(tfa);
  tft.writedata((YMAX-tfa-bfa)>>8);  // Vertical Scrolling Area line count
  tft.writedata(YMAX-tfa-bfa);
  tft.writedata(bfa >> 8);           // Bottom Fixed Area line count
  tft.writedata(bfa);
}

// ##############################################################################################
// Setup the vertical scrolling start address pointer
// ##############################################################################################
void scrollAddress(uint16_t vsp) {
  tft.writecommand(ILI9341_VSCRSADD); // Vertical scrolling pointer
  tft.writedata(vsp>>8);
  tft.writedata(vsp);
}

void updateFirmware() {
	File updateFile = SD.open(UPDATE_FILE);
	if (updateFile == NULL || updateFile.isDirectory()) {
		return;
	}
	
	size_t updateSize = updateFile.size();
	
	if(Update.begin(updateSize)) {
		tft.print("Update started");
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
