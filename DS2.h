#ifndef DS2_h
#define DS2_h
#if ARDUINO >= 100
  #include "Arduino.h"
#else
  #include "WProgram.h"
  #include "pins_arduino.h"
  #include "WConstants.h"
#endif

class DS2 {
  public:
    DS2(Stream &stream);
	boolean writeData(uint8_t data[]);
	boolean readCommand(uint8_t data[]);
    boolean readData(uint8_t data[]);
	boolean checkData(uint8_t data[]);
	
	
	
	uint8_t getDevice();
	uint8_t setDevice(uint8_t dev);
	
	void setBlocking(boolean mode);
	void clearRX();
	void clearRX(uint8_t available, uint8_t length);
  private:
    Stream serial;
	boolean blocking;
	boolean messageSend;
	uint8_t device;
	uint8_t echoLength, responseLength;
};

#endif