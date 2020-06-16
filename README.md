DS2 Library

*	Made to simplyfy the communication between arduino code and ECUs using DS2 k-line protocol ISO 9141.
	It was made and tested for L9637D but any interface should work the same way. 

*	Look at example and drop 'library' folder into your library location.

*	Echo - if you send any command on DS2 it responds with whatever was send due to way K-line is like contructed.
*	Imagine K-line being a single guitar string and our TX is making it vibrate and our RX is constantly picking up this vibrations. This is echo
	
*	1st byte in command/response is device which should receive command or which send the command
	0x12 - ECU default

*	2nd byte is command/response length

*	3rd byte for response it's acknowledge byte A0 meaning positive response, B0 being negative and any other (usually FF) being error; for command is specific to command itself.
	
*	4th - Xth bytes are for payload

*	Last byte is always XOR checksum. You can use this calculator to get it: https://www.scadacore.com/tools/programming-calculators/online-checksum-calculator/

*	Some commands for DS2 for BMW:
	ECU id - {0x12, 0x04, 0x00, 0x16}
	General values - {0x12, 0x05, 0x0B, 0x03, 0x1F}
	
	
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