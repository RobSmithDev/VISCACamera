#ifndef TELEPRESENCE_LCD_CONTROL_H
#define TELEPRESENCE_LCD_CONTROL_H

#include <Wire.h>
#include "LiquidCrystal_I2C.h"  // https://github.com/johnrickman/LiquidCrystal_I2C


class LCDDisplay {
	private:
		LiquidCrystal_I2C m_lcdPanel;
		
		// Used to make the display turn off when inactive
		const unsigned long m_displayTimeout;
		unsigned long m_lastUsed;
		bool m_isLit;
		
		// Sends somethign to the LCD pane and ensures its width characters wide - it does NOT position the cursor to start with
		void outputValue(const char* value, uint8_t width = 16, bool rightAlign = false);
		void outputValue(uint8_t value, uint8_t width);
		void outputValue(uint16_t value, uint8_t width);
		void outputValue(int8_t value, uint8_t width);
		void outputValue(int16_t value, uint8_t width);
		
		
		// Creates the left edge of the bar with numCols filled in from the left
		void createCustomCharWithXColumnsFromLeft(uint8_t charNum, uint8_t numCols, bool rightBar);

		// Generates a character with a specific range of columns filled
		void generateCharacterWithWithStratEndColumns(uint8_t charNum, uint8_t startCol, uint8_t endCol);

		// Creates the left edge of the bar with numCols filled in from the left
		void createCustomCharWithXColumnsFromRight(uint8_t charNum, uint8_t numCols, bool leftBar); 
		
	public:
		// Create the display 
		LCDDisplay(const uint8_t panelAddress = 0x27, const uint8_t columns = 16, const uint8_t rows = 2, const unsigned long displayTimeout = 30000);

    // Prepare the LCD panel
    void init();
	
		// Clear the display
		void clearDisplay();
		
		// Set the text on the first line
		void setHeading(const char* heading);
		
		// Set the text on the second row
		void setStatus(const char* status);
		
		// Needs to be called in the main function so the LCD switches off when not in use
		void monitorLCD();
		
		// Returns TRUE if the LCD was switched off, and powers it up
		bool lcdWasOff();
				
		// Outputs the value and a bar to show progress - We're only allowed 8 custom characters, so we have to use them carefully. 
		// If minValue is -ve then the bar assumes it swings each way from 0
		void outputProgressBar(int16_t value, const char* units, uint8_t valueWidth, int16_t minValue, int16_t maxValue);
};


#endif