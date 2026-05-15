
#include <arduino.h>
#include "LCDControl.h"

#define CUSTOMCHAR_TOPBOT 0
#define CUSTOMCHARS_SOLID 1
#define CUSTOMLEFT        2
#define CUSTOMRIGHT       3
#define CUSTOMMIDDLE1     4
#define CUSTOMMIDDLE2     5
#define CUSTOMMIDDLE3     6
#define CUSTOMMIDDLE4     7

// Create the display 
LCDDisplay::LCDDisplay(const uint8_t panelAddress, const uint8_t columns, const uint8_t rows, const unsigned long displayTimeout) 
	: m_lcdPanel(panelAddress, columns, rows), m_displayTimeout(displayTimeout) {	
}

// Prepare the LCD panel
void LCDDisplay::init() {
  m_lcdPanel.begin(16,2);  
	m_lcdPanel.init(); 
	m_lcdPanel.clear();
	m_lcdPanel.backlight();  
	
	// Handle the display
	m_lastUsed = millis();
	m_isLit = true;
	
	// Create two custom characters
	uint8_t customChar[8] = {B11111, 0, 0, 0, 0, 0, 0, B11111};
	m_lcdPanel.createChar(CUSTOMCHAR_TOPBOT, customChar);
	memset(&customChar[1], B11111, sizeof(customChar)-2);
	m_lcdPanel.createChar(CUSTOMCHARS_SOLID, customChar);
}


// Monitor the lcd and turn it off when idle
void LCDDisplay::monitorLCD() {
	if ((m_isLit) && (millis() - m_lastUsed>30000)) {
		m_isLit = false;
		m_lcdPanel.noBacklight();  
	}
}

// Returns TRUE if the LCD was switched off, and powers it up
bool LCDDisplay::lcdWasOff() {
	if (!m_isLit) {
		m_isLit = true;
    m_lcdPanel.backlight();
		m_lastUsed = millis();
		return true;
	} else {
		m_lastUsed= millis();
		return false;
	}
}

// Sends somethign to the LCD pane and ensures its width characters wide - it does NOT position the cursor to start with
void LCDDisplay::outputValue(const char* value, uint8_t width, bool rightAlign) {
	if (rightAlign) {
		// Clear anything before the new text
		uint8_t cursorPos = strlen(value);
		while (cursorPos++<width) m_lcdPanel.print(" ");      
	}
	m_lcdPanel.print(value);
	if (!rightAlign) {
		// Clear anything after the new text
		uint8_t cursorPos = strlen(value);
		while (cursorPos++<width) m_lcdPanel.print(" ");  
	}
}

void LCDDisplay::outputValue(uint8_t value, uint8_t width) {
	char tmp[4];
	itoa(value, tmp, 10);
	outputValue(tmp, width, true);
}

void LCDDisplay::outputValue(uint16_t value, uint8_t width) {
	char tmp[6];
	itoa(value, tmp, 10);
	outputValue(tmp, width, true);
}

void LCDDisplay::outputValue(int8_t value, uint8_t width) {
	char tmp[5];
	itoa(value, tmp, 10);
	outputValue(tmp, width, true);
}

void LCDDisplay::outputValue(int16_t value, uint8_t width) {
	char tmp[7];
	itoa(value, tmp, 10);
	outputValue(tmp, width, true);
}

// Clear the display
void LCDDisplay::clearDisplay() {
	m_lcdPanel.clear();
}

// Set the text on the first line
void LCDDisplay::setHeading(const char* heading) {
	m_lcdPanel.setCursor(0,0);
	outputValue(heading, 16);
}

// Set the text on the second row
void LCDDisplay::setStatus(const char* status) {
	m_lcdPanel.setCursor(0,1);
	outputValue(status, 16);
}

// Creates the left edge of the bar with numCols filled in from the left
void LCDDisplay::createCustomCharWithXColumnsFromLeft(uint8_t charNum, uint8_t numCols, bool rightBar) { 
	const uint8_t m = rightBar ? B00001 : 0;
	uint8_t customChar[8] = {B11111, m, m, m, m, m, m, B11111};
	if (numCols) {
		for (uint8_t c=0; c<numCols; c++) {
			customChar[1]|=1 << (4-c);
			for (uint8_t row=2; row<7; row++) 
				customChar[row] = customChar[1];
		}
	}
	m_lcdPanel.createChar(charNum, customChar);  
}

// Generates a character with a specific range of columns filled
void LCDDisplay::generateCharacterWithWithStratEndColumns(uint8_t charNum, uint8_t startCol, uint8_t endCol) {
	uint8_t customChar[8] = {B11111, 0,0,0,0,0,0, B11111};
	if (startCol>endCol) {
		uint8_t tmp = startCol;
		startCol = endCol;
		endCol = tmp;
	}
	for (uint8_t c=startCol; c<=endCol; c++) {
		customChar[1]|=1 << (4-c);
		for (uint8_t row=2; row<7; row++) 
			customChar[row] = customChar[1];
	}
	m_lcdPanel.createChar(charNum, customChar);  
}

// Creates the left edge of the bar with numCols filled in from the left
void LCDDisplay::createCustomCharWithXColumnsFromRight(uint8_t charNum, uint8_t numCols, bool leftBar) { 
	const uint8_t m = leftBar ? B10000 : 0;
	uint8_t customChar[8] = {B11111, m, m, m, m, m, m, B11111};
	if (numCols) {
		for (uint8_t c=0; c<numCols; c++) {
			customChar[1]|=1 << c;
			for (uint8_t row=2; row<7; row++) 
				customChar[row] = customChar[1];
		}
	}
	m_lcdPanel.createChar(charNum, customChar);  
}

// Outputs the value and a bar to show progress - We're only allowed 8 custom characters, so we have to use them carefully. 
// If minValue is -ve then the bar assumes it swings each way from 0
void LCDDisplay::outputProgressBar(int16_t value, const char* units, uint8_t valueWidth, int16_t minValue, int16_t maxValue) {
  m_lcdPanel.setCursor(0,1);
	outputValue(value, valueWidth);
	if (units) {
		m_lcdPanel.print(units);
		valueWidth+=strlen(units);
	}
	const uint8_t widthRemaining = ((16 - valueWidth) * 5) - 2; // the -2 is because we need 2 columns, one at the start and one at the end

	// Handle going negative with a centre point
	value -= minValue;  
	const int16_t range = maxValue - minValue;

	// If we update the character shown in the middle while its still being displayed the screen flickers with the new charactre and spoils the effect
	static bool toggleMiddleCharacter = false;
	toggleMiddleCharacter = !toggleMiddleCharacter;
	const uint8_t CustomMiddle = toggleMiddleCharacter ? CUSTOMMIDDLE1 : CUSTOMMIDDLE2;  

	// Does the bar have negative parts?  
	if (minValue>=0) {
		// Normal bar - Work out how many columns actually need filling
		uint8_t columns = constrain(((int32_t)value * (int32_t)widthRemaining) / (int32_t)range, 0, widthRemaining);

		// Left/Middle - The first block only has 4 columns.
		createCustomCharWithXColumnsFromLeft(CustomMiddle, (columns<4) ? columns+1 : (columns-4) % 5, false);
		// End bar. The last block only has 4 columns
		createCustomCharWithXColumnsFromLeft(CUSTOMRIGHT, constrain(columns - (widthRemaining - 4), 0, 4), true);

		// TIP: If we don't call setCursor here we end up writing into the memory used for the custom characters rather than the screen
		m_lcdPanel.setCursor(valueWidth,1);      
		m_lcdPanel.write((columns<4) ? CustomMiddle : CUSTOMCHARS_SOLID);  
		columns = constrain(columns-4,0,widthRemaining);

		// Populate the middle
		for (uint8_t p = valueWidth+1; p<15; p++) {
			if (columns == 0) m_lcdPanel.write(CUSTOMCHAR_TOPBOT); else      
			if (columns >= 5) {        
				columns -=5;
				m_lcdPanel.write(CUSTOMCHARS_SOLID);
			} else {       
				m_lcdPanel.write(CustomMiddle);
				columns = 0;
			}
		}

		// Now the right-hand piece    
		m_lcdPanel.write(CUSTOMRIGHT);
	} else {
		const uint8_t CustomMiddle2 = toggleMiddleCharacter ? CUSTOMMIDDLE3 : CUSTOMMIDDLE4;

		// Bar that swings left/right from the middle point
		uint8_t middleColumn = constrain(((int32_t)(-minValue) * (int32_t)widthRemaining) / (int32_t)range, 0, widthRemaining);
		uint8_t columnStart = constrain(((int32_t)value * (int32_t)widthRemaining) / (int32_t)range, 0, widthRemaining);

		// Create the first and last characters of the bar
		createCustomCharWithXColumnsFromRight(CUSTOMLEFT, 4-min(columnStart, 4), true);   
		createCustomCharWithXColumnsFromLeft(CUSTOMRIGHT, constrain(columnStart - (widthRemaining - 4), 0, 4), true);   

		const uint8_t charWithBarStart = (columnStart+1)/5;
		const uint8_t charWithMidPoint = (middleColumn+1)/5;

		// Handle the mid point and current point
		if (charWithBarStart == charWithMidPoint) {
			generateCharacterWithWithStratEndColumns(CustomMiddle, (columnStart-4) % 5, (middleColumn-4) % 5);
		} else {      
			generateCharacterWithWithStratEndColumns(CustomMiddle, (middleColumn-4) % 5, (columnStart<middleColumn) ? 0 : 4);            
			if (columnStart<middleColumn) createCustomCharWithXColumnsFromRight(CustomMiddle2, 4-((columnStart-4) % 5), false);   
				else createCustomCharWithXColumnsFromLeft(CustomMiddle2, (columnStart-4) % 5, false);   
		}

		// Prepare for output
		m_lcdPanel.setCursor(valueWidth,1);
		m_lcdPanel.write(CUSTOMLEFT);  

		// Write bar
		for (uint8_t p = valueWidth+1; p<15; p++) {
			const uint8_t currentChar = p - valueWidth;
			if (currentChar == charWithMidPoint) {
				m_lcdPanel.write(CustomMiddle);
			} else
			if (currentChar == charWithBarStart) {
				m_lcdPanel.write(CustomMiddle2);
			} else {
				m_lcdPanel.write((currentChar < charWithMidPoint) ^ (currentChar >= charWithBarStart) ? CUSTOMCHAR_TOPBOT : CUSTOMCHARS_SOLID);
			}
		}
		m_lcdPanel.write(CUSTOMRIGHT);  
	}
}

