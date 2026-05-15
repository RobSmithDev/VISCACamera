
#include "TelepresenceVisca.h"
#include "LCDControl.h"
#include <EEPROM.h>

#define PIN_ROTARY_CLK  2
#define PIN_ROTARY_DT   3
#define PIN_ROTARY_SW   4

#define PIN_PRESET0     5
#define PIN_PRESET1     6
#define PIN_PRESET2     7
#define PIN_PRESET3     8
#define PIN_PRESET4     9
#define PIN_PRESET5     10

#define PIN_CAMRX       11
#define PIN_CAMTX       12

#define PIN_SAVEPRESET  A0
#define PIN_JOY_SW      A1
#define PIN_JOYY        A3
#define PIN_JOYX        A2
#define PIN_SDA         A4
#define PIN_SDL         A5

#define CAMERA_ADDRESS 1

// How much the voltage read by the joysticks has to move before we notice a change
#define JOYSTICK_NOISE_GATE 8

// Modes used by the app
#define MAX_MODES 6
enum CurrentMode : uint8_t {cmZoom=0,cmPanTilt, cmFocus, cmWhiteBalance, cmExposure, cmFlip};
static const char* ModeNames[MAX_MODES] = { "Zoom","Pan/Tilt","Focus","White Balance","Exposure","H/V Flip" };
    
// Used to track movement on the shaft encoder
CurrentMode currentMode = CurrentMode::cmZoom;
#define VALID_MASK  B11001100

VISCATelepresence camera(PIN_CAMRX, PIN_CAMTX);

struct CameraCurrentConfig {
  uint8_t isValid;
  uint16_t zoom;    
  uint16_t focus;    
  bool autoFocus;    
  uint16_t pan;
  uint16_t tilt;
  
  bool autoWhiteBalance; 
  uint16_t whiteBalance; 
  bool backlightComp;  
  
  bool autoExposure;    

  bool hFlip, vFlip;

  bool autoGamma; 
  uint8_t gamma;
  uint8_t iris;
  uint8_t gain;
  uint8_t brightness;
} cameraSettings;

int16_t smallValueSlowdown;

struct {		  
  // Midpoints for the joystick
  uint16_t joyXMid;
  uint16_t joyYMid;
} lastSetValues;
				
LCDDisplay lcdPanel(0x27, 16, 2, 60000); // I2C address 0x27 (or 0x3F), 16 column and 2 rows

const VISCATelepresence::Limits* cameraLimits = NULL;

// Read the current settings from the camera
bool getCameraSettings() {
  if (!camera.getPanTilt(cameraSettings.pan, cameraSettings.tilt)) return false;
  if (!camera.getCameraZoom(cameraSettings.zoom)) return false;
  if (!camera.getFocusValue(cameraSettings.autoFocus, cameraSettings.focus)) return false;
  if (!camera.getWhiteBalanceValue(cameraSettings.autoWhiteBalance, cameraSettings.whiteBalance)) return false;
  if (!camera.getAutoExposure(cameraSettings.autoExposure)) return false;
  if (!camera.getBrightnessValue(cameraSettings.brightness)) return false;
  if (!camera.getHVFlip(cameraSettings.hFlip, cameraSettings.vFlip)) return false;

  return true;
}

// Write the current settings to the camera
bool setCameraSettings() {
  if (!camera.setAutoWhiteBalance(cameraSettings.autoWhiteBalance)) return false;
  if (!cameraSettings.autoWhiteBalance) {
    delay(100);
    if (!camera.setWhiteBalance(cameraSettings.whiteBalance)) return false;    
  }
  if (!camera.setAutoExposure(cameraSettings.autoExposure)) return false;
  if (!cameraSettings.autoExposure) {
    delay(100);
    if (!camera.setBrightnessValue(cameraSettings.brightness)) return false;
  }
  delay(100);
  if (!camera.setHVFlip(cameraSettings.hFlip, cameraSettings.vFlip)) return false;
  if (!camera.setAutoFocus(cameraSettings.autoFocus)) return false;
  if (!camera.setPanTiltZoomFocus(cameraSettings.pan, cameraSettings.tilt, cameraSettings.zoom, cameraSettings.focus)) return false;      
  delay(300);
  return true;
}

// Read the X position of the joystick.  The joystick already has a deadspot where it centers, but this will get rid of some of the A2D noise too
int16_t getJoyX() {
  int16_t i = analogRead(PIN_JOYX) - lastSetValues.joyXMid;
  if (i<-JOYSTICK_NOISE_GATE) return i+JOYSTICK_NOISE_GATE;
  if (i>JOYSTICK_NOISE_GATE) return i-JOYSTICK_NOISE_GATE;
  return 0;
}

// Read the Y position of the joystick.  The joystick already has a deadspot where it centers, but this will get rid of some of the A2D noise too
int16_t getJoyY() {
  int16_t i = analogRead(PIN_JOYY) - lastSetValues.joyYMid;
  if (i<-JOYSTICK_NOISE_GATE) return i+JOYSTICK_NOISE_GATE;
  if (i>JOYSTICK_NOISE_GATE) return i-JOYSTICK_NOISE_GATE;
  return 0;
}

// Displays on the second line the current setting value
void showCurrentValue() {
  switch (currentMode) {
    case CurrentMode::cmZoom: lcdPanel.outputProgressBar(cameraSettings.zoom-cameraLimits->ZOOM_MIN, NULL, 4, 0, cameraLimits->ZOOM_MAX-cameraLimits->ZOOM_MIN); break;
    case CurrentMode::cmPanTilt:  {
                char message[17];
                sprintf(message, "P: %04i T: %04i", cameraSettings.pan, cameraSettings.tilt);
                lcdPanel.setStatus(message);		
              }
            break;
    case CurrentMode::cmFocus: if (cameraSettings.autoFocus) lcdPanel.setStatus("Automatic Focus"); else lcdPanel.outputProgressBar(cameraSettings.focus-cameraLimits->FOCUS_NEAR, NULL, 3, 0, cameraLimits->FOCUS_FAR-cameraLimits->FOCUS_NEAR); break;
    case CurrentMode::cmExposure: if (cameraSettings.autoExposure) lcdPanel.setStatus("Automatic"); else lcdPanel.outputProgressBar(cameraSettings.brightness, NULL, 2, cameraLimits->BRIGHTNESS_MIN, cameraLimits->BRIGHTNESS_MAX); break;  
    case CurrentMode::cmWhiteBalance: if (cameraSettings.autoWhiteBalance)  lcdPanel.setStatus("Auto White Bal"); else lcdPanel.outputProgressBar(cameraSettings.whiteBalance-cameraLimits->WB_YELLOW, NULL, 2, 0, cameraLimits->WB_BLUE-cameraLimits->WB_YELLOW); break;
    case CurrentMode::cmFlip: {
								char message[17];
                strcpy(message, "H: ");
								strcat(message, cameraSettings.hFlip?"Flip":"Norm");
								strcat(message, " V: "); 
								strcat(message, cameraSettings.vFlip?"Flip":"Norm");
                lcdPanel.setStatus(message);							  
							  }
                break;    
  }
}

// Joystick button click either toggles or resets back to a preset point
void joystickButtonPressed() {
  switch (currentMode) {
    case CurrentMode::cmZoom: 
        cameraSettings.zoom = cameraLimits->ZOOM_MAX;   // Zoom out furthest
        camera.setCameraZoom(cameraSettings.zoom);
        showCurrentValue();
        break;
    case CurrentMode::cmPanTilt:
         camera.resetPanTilt();
         delay(1000);
         camera.getPanTilt(cameraSettings.pan, cameraSettings.tilt);
         showCurrentValue();
         break;
    case CurrentMode::cmFocus:         
        if (cameraSettings.autoFocus) modeChanged();  // cause the manual focus value to be re-read
        cameraSettings.autoFocus = !cameraSettings.autoFocus;    // Toggle AutoFocus
        camera.setAutoFocus(cameraSettings.autoFocus);
        showCurrentValue();
        break;
    case CurrentMode::cmWhiteBalance: 
        if (cameraSettings.autoWhiteBalance) modeChanged();  // cause the manual focus value to be re-read
        cameraSettings.autoWhiteBalance = !cameraSettings.autoWhiteBalance;   // Toggle Auto-White Balance
        camera.setAutoWhiteBalance(cameraSettings.autoWhiteBalance);
        showCurrentValue();
        break;        
    case CurrentMode::cmExposure: 
        cameraSettings.autoExposure = !cameraSettings.autoExposure;
        camera.setAutoExposure(cameraSettings.autoExposure);                   // Toggle Auto Exposure
        camera.setBrightnessValue(cameraSettings.brightness);
        showCurrentValue();
        break;
    case CurrentMode::cmFlip: 
        cameraSettings.hFlip = false;
        cameraSettings.vFlip = false;
        camera.setHVFlip(cameraSettings.hFlip, cameraSettings.vFlip); 
        showCurrentValue();
        break;           
  }
}

// Does the same thing
void shaftButtonPressed() {
  joystickButtonPressed();
}

// Displays the currently selected mode on the LCD panel
void showCurrentMode() {
	lcdPanel.setHeading(ModeNames[currentMode]);
}

// Called when the current mode changes
void modeChanged() {
  // Fetch the current value for each thing
  switch (currentMode) {
    case CurrentMode::cmZoom: camera.getCameraZoom(cameraSettings.zoom); break;
    case CurrentMode::cmPanTilt:  camera.getPanTilt(cameraSettings.pan, cameraSettings.tilt); break;
    case CurrentMode::cmFocus: camera.getFocusValue(cameraSettings.autoFocus, cameraSettings.focus); break;
    case CurrentMode::cmWhiteBalance: 
                camera.getWhiteBalanceValue(cameraSettings.autoWhiteBalance, cameraSettings.whiteBalance); 
                smallValueSlowdown = (uint16_t)cameraSettings.whiteBalance * 100; 
                break;
    case CurrentMode::cmExposure: 
                      camera.getAutoExposure(cameraSettings.autoExposure); 
                      camera.getBrightnessValue(cameraSettings.brightness); 
                      smallValueSlowdown = (uint16_t)cameraSettings.brightness * 100;
                      break;
    case CurrentMode::cmFlip: camera.getHVFlip(cameraSettings.hFlip, cameraSettings.vFlip); break;
  }
  showCurrentValue();
}

void handleJoystickPosition(int16_t xPosition, int16_t yPosition) {
  switch (currentMode) {
    case CurrentMode::cmZoom: {
        uint16_t lastZoom = cameraSettings.zoom;
        cameraSettings.zoom = constrain((int16_t)cameraSettings.zoom + ((xPosition - yPosition)/4), (int16_t)cameraLimits->ZOOM_MIN, (int16_t)cameraLimits->ZOOM_MAX);
        if (lastZoom != cameraSettings.zoom) {
          camera.setCameraZoom(cameraSettings.zoom);
          showCurrentValue();
        }
      }
      break;
    case CurrentMode::cmPanTilt: {
        static bool wasMoving = false;
        bool moving;
		    camera.setPanTiltSpeed(constrain(xPosition/32,-15,15), constrain(-yPosition/32, -15, 15), moving);
        if ((moving != wasMoving) && (!moving)) {
          uint16_t lastPan = cameraSettings.pan;
          uint16_t lastTilt = cameraSettings.tilt;
          camera.getPanTilt(cameraSettings.pan, cameraSettings.tilt);
          if ((cameraSettings.pan != lastPan) || (cameraSettings.tilt != lastTilt)) {       
            showCurrentValue();
          }
        }        
        wasMoving= moving;
      }
      break;
    case CurrentMode::cmFocus: {
        uint16_t lastFocus = cameraSettings.focus;
        cameraSettings.focus = constrain((int16_t)cameraSettings.focus + ((xPosition + yPosition)/16),(int16_t) cameraLimits->FOCUS_NEAR,(int16_t) cameraLimits->FOCUS_FAR);
        if (lastFocus != cameraSettings.focus) {
          camera.setFocusValue(cameraSettings.focus);
          showCurrentValue();
        }
      }
      break;      
    case CurrentMode::cmExposure: {
        uint16_t lastExposure = cameraSettings.brightness;
        smallValueSlowdown = constrain((int16_t)smallValueSlowdown + ((xPosition + yPosition)/16), (int16_t)cameraLimits->BRIGHTNESS_MIN*100, (int16_t)cameraLimits->BRIGHTNESS_MAX*100);
        cameraSettings.brightness = smallValueSlowdown / 100;
        if (lastExposure != cameraSettings.brightness) {
          camera.setBrightnessValue(cameraSettings.brightness);
          showCurrentValue();
        }
      }
      break;                   
    case CurrentMode::cmWhiteBalance: {
        uint16_t lastWhiteBalance = cameraSettings.whiteBalance;
        smallValueSlowdown = constrain((int16_t)smallValueSlowdown + ((xPosition + yPosition)/16), (int16_t)cameraLimits->WB_YELLOW*100, (int16_t)cameraLimits->WB_BLUE*100);
        cameraSettings.whiteBalance = smallValueSlowdown / 100;
        if (lastWhiteBalance != cameraSettings.whiteBalance) {
          camera.setWhiteBalance(cameraSettings.whiteBalance);
          showCurrentValue();
        }
      }
      break;         
  case CurrentMode::cmFlip: {
        bool lasthFlip = cameraSettings.hFlip;
        bool lastvFlip = cameraSettings.vFlip;
        if ((xPosition/4)!=0) cameraSettings.hFlip = xPosition < 0;
        if ((yPosition/4)!=0) cameraSettings.vFlip = yPosition < 0;
        if ((cameraSettings.hFlip != lasthFlip) || (cameraSettings.vFlip != lastvFlip)) {
          camera.setHVFlip(cameraSettings.hFlip, cameraSettings.vFlip);
          showCurrentValue();
        }
      }
      break;   
    default:
      break;      
  }
}

// Special accurate mode
void advanceOnce(int16_t delta) {
  switch (currentMode) {
    case CurrentMode::cmZoom: {
        uint16_t lastZoom = cameraSettings.zoom;
        cameraSettings.zoom = constrain((int16_t)cameraSettings.zoom + delta, (int16_t)cameraLimits->ZOOM_MIN, (int16_t)cameraLimits->ZOOM_MAX);
        if (lastZoom != cameraSettings.zoom) {
          camera.setCameraZoom(cameraSettings.zoom);
          showCurrentValue();
        }
      }
      break;   
    case CurrentMode::cmFocus: {
        uint16_t lastFocus = cameraSettings.focus;
        cameraSettings.focus = constrain((int16_t)cameraSettings.focus + delta, (int16_t)cameraLimits->FOCUS_NEAR, (int16_t)cameraLimits->FOCUS_FAR);
        if (lastFocus != cameraSettings.focus) {
          camera.setFocusValue(cameraSettings.focus);
          showCurrentValue();
        }
      }
      break;    
      case CurrentMode::cmExposure: {
        uint16_t lastBrightness = cameraSettings.brightness;
        smallValueSlowdown = constrain(smallValueSlowdown + ((int16_t)delta*100), (int16_t)cameraLimits->BRIGHTNESS_MIN*100, (int16_t)cameraLimits->BRIGHTNESS_MAX*100);
        cameraSettings.brightness = smallValueSlowdown / 100;
        if (lastBrightness != cameraSettings.brightness) {
          camera.setBrightnessValue(cameraSettings.brightness);
          showCurrentValue();
        }
      }
      break;          
    case CurrentMode::cmWhiteBalance: {
        uint16_t lastWhiteBalance = cameraSettings.whiteBalance;
        smallValueSlowdown = constrain(smallValueSlowdown + ((int16_t)delta*100), (int16_t)cameraLimits->WB_YELLOW*100, (int16_t)cameraLimits->WB_BLUE*100);
        cameraSettings.whiteBalance = smallValueSlowdown / 100;
        if (lastWhiteBalance != cameraSettings.whiteBalance) {
          camera.setWhiteBalance(cameraSettings.whiteBalance);
          showCurrentValue();
        }
      }
      break;              
    default:
      break;      
  }
}

// Arduino setup function
void setup() {  
  lcdPanel.init();
  lcdPanel.setHeading("Waiting for");
  lcdPanel.setStatus("Camera...");

  Serial.begin(9600);

  pinMode(PIN_SAVEPRESET, INPUT_PULLUP);
  pinMode(PIN_PRESET0, INPUT_PULLUP);
  pinMode(PIN_PRESET1, INPUT_PULLUP);
  pinMode(PIN_PRESET2, INPUT_PULLUP);
  pinMode(PIN_PRESET3, INPUT_PULLUP);
  pinMode(PIN_PRESET4, INPUT_PULLUP);
  pinMode(PIN_PRESET5, INPUT_PULLUP);
  pinMode(PIN_PRESET5, INPUT_PULLUP);

  pinMode(PIN_JOY_SW, INPUT_PULLUP);
  pinMode(PIN_ROTARY_DT, INPUT);
  pinMode(PIN_ROTARY_CLK, INPUT);
  
  pinMode(PIN_ROTARY_SW, INPUT_PULLUP);
  pinMode(PIN_JOYX, INPUT);
  pinMode(PIN_JOYY, INPUT);

  // Show we have control
  bool shouldReboot = true;
  while (!camera.initialise(CAMERA_ADDRESS, shouldReboot)) {
    shouldReboot = false;
    delay(350);
  };  

  // Setup the output
  lcdPanel.clearDisplay(); 
  char tmp[126];
  if (camera.getCameraSerialNumber(tmp, 126)) {
    Serial.print("Camera Serial Number: ");
    Serial.println(tmp);

    // This is the serial number of my ct1080p12xS2 so i'm using it to identify that model
    bool is12x = (strcmp(tmp,"A1AB02300356")==0);
    camera.setCameraMode(is12x ? VISCATelepresence::CameraType::ct1080p12xS2 : VISCATelepresence::CameraType::ct1080p4xS2);
    if (is12x) Serial.println("Serial is 1080p12xS2");
  } 
  if (camera.getCameraSoftwareVersion(tmp, 126)) {
    Serial.print("Camera Software Version: ");
    Serial.println(tmp);
  }

  cameraLimits = camera.getCameraLimits();

  // Get average readings for the joystick
  lastSetValues.joyXMid=0;
  lastSetValues.joyYMid=0;
  for (uint8_t a=0; a<10; a++) {
    lastSetValues.joyXMid += analogRead(PIN_JOYX);
    lastSetValues.joyYMid += analogRead(PIN_JOYY);
  }
  lastSetValues.joyXMid/=10;
  lastSetValues.joyYMid/=10;

  // Load the first preset on startup
  loadPreset(0);
  showCurrentMode();
  modeChanged();
}

// Check the state of any of the inputs
void checkShaftEncoder() {
  if (digitalRead(PIN_ROTARY_CLK) == LOW) {    
    bool dataState = digitalRead(PIN_ROTARY_DT) == HIGH;    
    // Dirty debouncing    
    delay(50);
    while (digitalRead(PIN_ROTARY_CLK) == LOW) {};
    delay(50);
           
    if (lcdPanel.lcdWasOff()) return;
    
    if (digitalRead(PIN_SAVEPRESET) == LOW) {
      advanceOnce(dataState?1:-1);
      return;
    } else {
      if (dataState) {
        // Clockwise
        currentMode = (CurrentMode)((uint8_t)(currentMode+1) % MAX_MODES);
      } else {
        // anticlockwise
        currentMode = (CurrentMode)(((uint8_t)currentMode == 0) ? MAX_MODES-1 : (uint8_t)currentMode-1);
      }
    }
    modeChanged();
    showCurrentMode();
    showCurrentValue();
  } 

  // See if the button is pressed
  if (digitalRead(PIN_ROTARY_SW) == LOW) {
    // Handle the LCD
    if (!lcdPanel.lcdWasOff()) shaftButtonPressed();

    // Dirty switch debouncing
    delay(50);
    while (digitalRead(PIN_ROTARY_SW) == LOW) {};
    delay(50);
  }
}

// Handle the joystick
void monitorJoystick() {
  int16_t joyX = getJoyX();
  int16_t joyY = getJoyY();

  // Passive debouncing of the joystick button
  static bool joyBtnInitiallyPressed = false;
  static unsigned long long whenPressed = 0;
  if (digitalRead(PIN_JOY_SW) == LOW) {
    whenPressed = millis();
    if (!joyBtnInitiallyPressed) {
      if (!lcdPanel.lcdWasOff()) joystickButtonPressed();      
      joyBtnInitiallyPressed = true;
    }     
  } else 
    if ((joyBtnInitiallyPressed) && (millis() - whenPressed>100)) 
      joyBtnInitiallyPressed = false;

  // Wake up the LCD
  if ((joyX !=0) || (joyY != 0)) lcdPanel.lcdWasOff();  
  handleJoystickPosition(joyX, joyY);
}

// Load preset from EEPROM
void loadPreset(uint8_t presetPin) {
  char tmp[16];
  char tmp2[5];
  strcpy(tmp,"Load Preset ");
  itoa(presetPin+1, tmp2, 10);
  strcat(tmp, tmp2);
  lcdPanel.setHeading(tmp);
  lcdPanel.setStatus("Please Wait...");

  int address = sizeof(cameraSettings) * (int)presetPin;
  EEPROM.get(address, cameraSettings);
  if (cameraSettings.isValid != VALID_MASK) {
    getCameraSettings();
    lcdPanel.setStatus("No Preset Found");
  } else lcdPanel.setStatus("Loaded OK");
  setCameraSettings();
  delay(1000);
  modeChanged();
}

// Save preset to EEPROM
void savePreset(uint8_t presetPin) {
  char tmp[16];
  char tmp2[5];

  strcpy(tmp,"Save Preset ");
  itoa(presetPin+1, tmp2, 10);
  strcat(tmp, tmp2);

  lcdPanel.setHeading(tmp);
  lcdPanel.setStatus("Please Wait...");

  if (!getCameraSettings()) {
    lcdPanel.setStatus("Camera IO Failed");
    delay(2000);
    return;
  }

  int address = sizeof(cameraSettings) * (int)presetPin;
  cameraSettings.isValid = VALID_MASK;
  EEPROM.put(address, cameraSettings);
  lcdPanel.setStatus("Saved OK");
  delay(1000);
}

// Monitor the preset buttons
void monitorPresetButtons() {
  bool isSavePressed = digitalRead(PIN_SAVEPRESET) == LOW;

  for (uint8_t presetPin = PIN_PRESET0; presetPin<=PIN_PRESET5; presetPin++) {
    // Is a preset button pressed?
    if (digitalRead(presetPin) == LOW) {

      // Load/Save the preset
      if (!lcdPanel.lcdWasOff()) {
        if (isSavePressed) savePreset(presetPin-PIN_PRESET0); else loadPreset(presetPin-PIN_PRESET0);
      }

      // Lazy debounding
      delay(50);
      while (digitalRead(presetPin) == LOW) {};
      delay(50);

      // Restore display
      showCurrentMode();
      showCurrentValue();

      return;
    }
  }
}

void loop() {
  // Watch inputs
  checkShaftEncoder();  
  monitorJoystick();
  monitorPresetButtons();
  
  // Check if the LCD should power off
  lcdPanel.monitorLCD();  
  Serial.print("1\n");
}
