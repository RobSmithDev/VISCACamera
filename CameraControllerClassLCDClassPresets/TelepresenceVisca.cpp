
#include <arduino.h>
#include "TelepresenceVisca.h"

VISCATelepresence::VISCATelepresence(uint8_t CameraRXPin, uint8_t cameraTXPin) 
	: m_serialPort(CameraRXPin, cameraTXPin) {
	m_serialPort.begin(9600);
	while (!m_serialPort) {};
  setCameraMode(VISCATelepresence::ctUnknown);
}


// Changes the limits on the functions. ctUnknown removes limits 
void VISCATelepresence::setCameraMode(VISCATelepresence::CameraType mode) {
  switch (mode) {
    case VISCATelepresence::CameraType::ct1080p4xS2:
        setLimits(64,1696,880,  16,440,264,   4096,11296,  64,574,  1,16,  0,50,  12,21,  0,7,1,45);
        break;
    
    case VISCATelepresence::CameraType::ct1080p12xS2:
        setLimits(0,816,408,   6,212,126,   0,2885,  4096,4672,  1,16,  0,200,  12,21,  0,7,1,45);
        break;
    default:
        setLimits(0,0xFFFF,0x7FFF,0,0xFFFF,0x7FFF,0,0xFFFF,0,0xFFFF,0,0xFF,0,0xFF,0,0xFF,0,0xFF,0,0xFF);
        break;
  }
}

// Yeah a little nasty
void VISCATelepresence::setLimits(uint16_t PAN_MIN,uint16_t PAN_MAX,uint16_t PAN_MIDDLE,uint16_t TILT_MIN,uint16_t TILT_MAX,uint16_t TILT_MIDDLE,uint16_t ZOOM_MIN,uint16_t ZOOM_MAX,uint16_t FOCUS_NEAR,uint16_t FOCUS_FAR,
               uint16_t WB_YELLOW,uint16_t WB_BLUE,uint16_t IRIS_MIN,uint16_t IRIS_MAX,uint16_t GAIN_MIN,uint16_t GAIN_MAX,uint8_t GAMMA_MIN,uint8_t GAMMA_MAX, uint8_t BRIGHTNESS_MIN, uint8_t BRIGHTNESS_MAX) {
  m_limits.PAN_MIDDLE = PAN_MIDDLE;
  m_limits.TILT_MIDDLE =TILT_MIDDLE;
  m_limits.PAN_MIN =    PAN_MIN;
  m_limits.PAN_MAX =    PAN_MAX;
  m_limits.TILT_MIN =   TILT_MIN;
  m_limits.TILT_MAX =   TILT_MAX;
  m_limits.ZOOM_MIN =   ZOOM_MIN;
  m_limits.ZOOM_MAX =   ZOOM_MAX;
  m_limits.FOCUS_NEAR = FOCUS_NEAR;
  m_limits.FOCUS_FAR =  FOCUS_FAR;
  m_limits.WB_YELLOW =  WB_YELLOW;
  m_limits.WB_BLUE =    WB_BLUE;
  m_limits.IRIS_MIN =   IRIS_MIN;
  m_limits.IRIS_MAX =   IRIS_MAX;
  m_limits.GAIN_MIN =   GAIN_MIN;
  m_limits.GAIN_MAX =   GAIN_MAX;
  m_limits.GAMMA_MIN =  GAMMA_MIN;
  m_limits.GAMMA_MAX =  GAMMA_MAX;
  m_limits.BRIGHTNESS_MIN =  BRIGHTNESS_MIN;
  m_limits.BRIGHTNESS_MAX =  BRIGHTNESS_MAX;
}

// Get camera limits
const VISCATelepresence::Limits* VISCATelepresence::getCameraLimits() {
  return &m_limits;
}

// Talk to the device and get a response
bool VISCATelepresence::exchangeCommand(VISCACommand& command, unsigned long timeout) {	
#ifdef ENABLE_VISCA_DEBUG_MESSAGES 
	m_serialPort.listen();
	Serial.print("Sending: ");
	for (int i=0; i<command.len; i++) {
	Serial.print(command.payload[i], HEX);
	Serial.print(" ");
	}
	Serial.println();
#endif	
	
	// Clear input buffer
	while (m_serialPort.available()) {m_serialPort.read();};
	// Send command
	m_serialPort.write(command.payload, command.len);
	// Wait for send
	m_serialPort.flush();

	unsigned long long startTime = millis();

	uint8_t c = 0;
	command.len = 0;
#ifdef ENABLE_VISCA_DEBUG_MESSAGES	
	Serial.print("Receiving: ");
#endif		
	do {
		if (m_serialPort.available()) {
			c = m_serialPort.read();
#ifdef ENABLE_VISCA_DEBUG_MESSAGES			 
			Serial.print(c, HEX);
			Serial.print(" ");
#endif			
			command.payload[command.len++] = c;
		} else
			if (millis() - startTime > timeout) {
#ifdef ENABLE_VISCA_DEBUG_MESSAGES				 
				Serial.println("timeout.");
#endif				
				return false;				
			}
	} while ((c!=0xFF) && (command.len<VISCACOMMAND_MAX_LENGTH));	
	
#ifdef ENABLE_VISCA_DEBUG_MESSAGES	 
	Serial.println();
	Serial.println();
#endif	
	return (command.len>=3) && (command.payload[command.len-1] == 0xFF);
}

// Initialises the camera, optionally resetting it, and ensures all default values have been provided
bool VISCATelepresence::initialise(uint8_t cameraAddress, bool performReset) {
  if (performReset) setCallLED(false);
	if (!setCameraAddress(cameraAddress)) return false;
  if (performReset) {
    if (!resetCamera()) return false;
    // The reset takes a while and we don't want the rest of the commands to time out - allow 10 second max
    for (uint8_t retry = 0; retry<40; retry++) {
      delay(250);    
      // if enabled, pan/tilt speed cant be changed
      if (enableIR(false)) break;
    }
  }
  delay(100);

	// Apply some default config settings, these are mostly the ones that we can't read back anyway	
	if (!setAutoGammaMode(false)) return false;
	if (!setAutoExposure(false)) return false;
	if (!setAutoWhiteBalance(true)) return false;
	if (!setGammaValue(0)) return false;
	if (!setIrisValue(25)) return false;
	if (!setGainValue(0)) return false;
  if (!setAutoGammaMode(true)) return false;
  if (!setAutoExposure(true)) return false;  
  if (!setBrightnessValue(10)) return false;
 // delay(400);
 // if (!resetPanTilt()) return false;
 // delay(1000);
  return setCallLED(true);  // ready!
}

// Attempt to identify the camera serial number
bool VISCATelepresence::getCameraSerialNumber(char* serial, uint8_t maxLength) {
  VISCACommand cmd;
  cmd.len = 5;
  cmd.payload[0] = 0x80 | m_cameraAddress;
  cmd.payload[1] = 0x09;
  cmd.payload[2] = 0x04;
  cmd.payload[3] = 0x24;
  cmd.payload[4] = 0xFF;
  if (!exchangeCommand(cmd)) return false;
  if (cmd.len<3) return false;
  if (cmd.payload[1] != 0x50) return false;

  uint16_t l = cmd.len - 3;
  if (l>(uint16_t)(maxLength-1)) l=(uint8_t)(maxLength-1);
  memcpy(serial, &cmd.payload[2], l);
  serial[l] = '\0';

  return true;
}

// Attempt to identify the camera serial number
bool VISCATelepresence::getCameraSoftwareVersion(char* softwareVersion, uint8_t maxLength) {
  VISCACommand cmd;
  cmd.len = 5;
  cmd.payload[0] = 0x80 | m_cameraAddress;
  cmd.payload[1] = 0x09;
  cmd.payload[2] = 0x04;
  cmd.payload[3] = 0x23;
  cmd.payload[4] = 0xFF;
  if (!exchangeCommand(cmd)) return false;
  if (cmd.len<3) return false;
  if (cmd.payload[1] != 0x50) return false;

  uint16_t l = cmd.len - 3;
  if (l>(uint16_t)(maxLength-1)) l=(uint8_t)(maxLength-1);
  memcpy(softwareVersion, &cmd.payload[2], l);
  softwareVersion[l] = '\0';

  return true;
}

// Runs a command that sets a single byte value
bool VISCATelepresence::do_uint8_command(uint8_t cl, uint8_t cmdl, uint8_t value) {
	VISCACommand cmd;
	cmd.len = 6;
	cmd.payload[0] = 0x80 | m_cameraAddress;
	cmd.payload[1] = 0x01;
	cmd.payload[2] = cl;
	cmd.payload[3] = cmdl;
	cmd.payload[4] = value;
	cmd.payload[5] = 0xFF;
	if (!exchangeCommand(cmd)) return false;
	return (cmd.len == 3) && (cmd.payload[1] == 0x50);
}

// Runs a query that fetches 16-bit value
bool VISCATelepresence::do_uint8_query(uint8_t cl, uint8_t cmdl, uint8_t& output) {
	VISCACommand cmd;
	cmd.len = 5;
	cmd.payload[0] = 0x80 | m_cameraAddress;
	cmd.payload[1] = 0x09;
	cmd.payload[2] = cl;
	cmd.payload[3] = cmdl;
	cmd.payload[4] = 0xFF;
	if (!exchangeCommand(cmd)) return false;
	if (cmd.payload[1] != 0x50) return false;
	if (cmd.len != 4) return false;
	output = cmd.payload[2];
	return true;
}
	
// Runs a query that fetches 16-bit value
bool VISCATelepresence::do_uint16_query(uint8_t cl, uint8_t cmdl, uint16_t& output) {
	VISCACommand cmd;
	cmd.len = 5;
	cmd.payload[0] = 0x80 | m_cameraAddress;
	cmd.payload[1] = 0x09;
	cmd.payload[2] = cl;
	cmd.payload[3] = cmdl;
	cmd.payload[4] = 0xFF;
	if (!exchangeCommand(cmd)) return false;
	if (cmd.payload[1] != 0x50) return false;
	if (cmd.len != 7) return false;
	output = (cmd.payload[5] & 0x0F) | ((cmd.payload[4] & 0x0F) << 4) | ((cmd.payload[3] & 0x0F) << 8) | ((cmd.payload[2] & 0x0F) << 12);	
	return true;
}

// Runs a command that sets a 16-bit value
bool VISCATelepresence::do_uint16_command(uint8_t cl, uint8_t cmdl, uint16_t value) {
	VISCACommand cmd;
	cmd.len = 9;
	cmd.payload[0] = 0x80 | m_cameraAddress;
	cmd.payload[1] = 0x01;
	cmd.payload[2] = cl;
	cmd.payload[3] = cmdl;
	cmd.payload[4] = value >> 12;
	cmd.payload[5] = (value >> 8) & 0x0F;
	cmd.payload[6] = (value >> 4) & 0x0F;
	cmd.payload[7] = value & 0x0F;
	cmd.payload[8] = 0xFF;
	if (!exchangeCommand(cmd)) return false;
	if (cmd.payload[1] != 0x50) return false;
	if (cmd.len != 3) return false;
	return true;
}

// Set the camera to be on a specific address
bool VISCATelepresence::setCameraAddress(uint8_t address) {
	VISCACommand cmd;
	cmd.len = 4;
	cmd.payload[0] = 0x88;// broadcast
	cmd.payload[1] = 0x30;
	cmd.payload[2] = address;
	cmd.payload[3] = 0xFF;
	if (!exchangeCommand(cmd)) return false;
	if (cmd.len == 4) {
		m_cameraAddress = address;
		return true;
	}
  return false;
}

// Sends a camera reboot message
bool VISCATelepresence::resetCamera() {
	VISCACommand cmd;
	cmd.len = 4;
	cmd.payload[0] = 0x80 | m_cameraAddress;
	cmd.payload[1] = 0x01;
	cmd.payload[2] = 0x42;
	cmd.payload[3] = 0xFF;

	if (!exchangeCommand(cmd)) return false;

	return (cmd.len == 3) && (cmd.payload[1] == 0x50); 
}

// Enable/Disables the IR TX/RX
bool VISCATelepresence::enableIR(bool enable) {
	if (!do_uint8_command(0x6, 0x8, enable?2:3)) return false;
	return do_uint8_command(0x6, 0x9, enable?2:3);
}

// Control picture flip
bool VISCATelepresence::setHVFlip(bool hFlip, bool vFlip) {
	if (!do_uint8_command(0x04, 0x61, hFlip?2:3)) return false;
	return do_uint8_command(0x04, 0x66, vFlip?2:3);
}

bool VISCATelepresence::getHVFlip(bool& hFlip, bool& vFlip) {
	uint8_t tmp;
	if (!do_uint8_query(0x04, 0x61, tmp)) return false;
	hFlip = tmp==2;
	if (!do_uint8_query(0x04, 0x66, tmp)) return false;
	vFlip = tmp==2;
	return true;
}

// Change the call led
bool VISCATelepresence::setCallLED(bool enabled) {
	return do_uint8_command(0x33, 0x01, enabled?1:0);
}

// Resets the pan/tilt to center, which also updates motor sync
bool VISCATelepresence::resetPanTilt() {
	VISCACommand cmd;
	cmd.len = 5;
	cmd.payload[0] = 0x80 | m_cameraAddress;
	cmd.payload[1] = 0x01;
	cmd.payload[2] = 0x06;
	cmd.payload[3] = 0x05;
	cmd.payload[4] = 0xFF;

	if (!exchangeCommand(cmd)) return false;

	return (cmd.len == 3) && (cmd.payload[1] == 0x50);
}

// Special command that sets all four things in one go
bool VISCATelepresence::setPanTiltZoomFocus(uint16_t pan, uint16_t tilt, uint16_t zoom, uint16_t focus) {
  pan = constrain(pan,m_limits.PAN_MIN,m_limits.PAN_MAX);
  tilt = constrain(tilt,m_limits.TILT_MIN,m_limits.TILT_MAX);
  zoom = constrain(zoom, m_limits.ZOOM_MIN, m_limits.ZOOM_MAX);
  focus = constrain(focus,m_limits.FOCUS_NEAR,m_limits.FOCUS_FAR);

  VISCACommand cmd;
  cmd.len = 21;
  cmd.payload[0] = 0x80 | m_cameraAddress;
  cmd.payload[1] = 0x01;
  cmd.payload[2] = 0x06;
  cmd.payload[3] = 0x20;
  cmd.payload[4] = pan >>12;
  cmd.payload[5] = (pan >> 8) & 0x0F;
  cmd.payload[6] = (pan >> 4) & 0x0F;
  cmd.payload[7] = pan & 0x0F;
  cmd.payload[8] = tilt >>12;
  cmd.payload[9] = (tilt >> 8) & 0x0F;
  cmd.payload[10] = (tilt >> 4) & 0x0F;
  cmd.payload[11] = tilt & 0x0F;
  cmd.payload[12] = zoom >>12;
  cmd.payload[13] = (zoom >> 8) & 0x0F;
  cmd.payload[14] = (zoom >> 4) & 0x0F;
  cmd.payload[15] = zoom & 0x0F;
  cmd.payload[16] = focus >>12;
  cmd.payload[17] = (focus >> 8) & 0x0F;
  cmd.payload[18] = (focus >> 4) & 0x0F;
  cmd.payload[19] = focus & 0x0F;
  cmd.payload[20] = 0xFF;

  if (!exchangeCommand(cmd)) return false;
  return (cmd.len == 3) && (cmd.payload[1] == 0x50);
}

// Set the current Pan and Tilt position
bool VISCATelepresence::setPanTilt(uint16_t pan, uint16_t tilt) {
  pan = constrain(pan,m_limits.PAN_MIN,m_limits.PAN_MAX);
  tilt = constrain(tilt,m_limits.TILT_MIN,m_limits.TILT_MAX);
	VISCACommand cmd;
	cmd.len = 15;
	cmd.payload[0] = 0x80 | m_cameraAddress;
	cmd.payload[1] = 0x01;
	cmd.payload[2] = 0x06;
	cmd.payload[3] = 0x02;
	cmd.payload[4] = 0x1F;	 // Pan Speed
	cmd.payload[5] = 0x1F;	 // Tilt Speed
	cmd.payload[6] = pan >>12;
	cmd.payload[7] = (pan >> 8) & 0x0F;
	cmd.payload[8] = (pan >> 4) & 0x0F;
	cmd.payload[9] = pan & 0x0F;
	cmd.payload[10] = tilt >>12;
	cmd.payload[11] = (tilt >> 8) & 0x0F;
	cmd.payload[12] = (tilt >> 4) & 0x0F;
	cmd.payload[13] = tilt & 0x0F;
	cmd.payload[14] = 0xFF;
	
	if (!exchangeCommand(cmd)) return false;
	return (cmd.len == 3) && (cmd.payload[1] == 0x50);
}

// Set the current Pan and Tilt position
bool VISCATelepresence::getPanTilt(uint16_t& pan, uint16_t& tilt) {
	VISCACommand cmd;
	cmd.len = 5;
	cmd.payload[0] = 0x80 | m_cameraAddress;
	cmd.payload[1] = 0x09;
	cmd.payload[2] = 0x06;
	cmd.payload[3] = 0x12;
	cmd.payload[4] = 0xFF;	
	if (!exchangeCommand(cmd)) return false;
	if (cmd.len!=11) return false;
	if (cmd.payload[1] != 0x50) return false;
	pan = (cmd.payload[5] & 0x0F) | ((cmd.payload[4] & 0x0F) << 4) | ((cmd.payload[3] & 0x0F) << 8) | ((cmd.payload[2] & 0x0F) << 12);	
	tilt = (cmd.payload[9] & 0x0F) | ((cmd.payload[8] & 0x0F) << 4) | ((cmd.payload[7] & 0x0F) << 8) | ((cmd.payload[6] & 0x0F) << 12);	
	return true;
}

// Change the current zoom level.	Range is 4096 to 11296, range is 7200
bool VISCATelepresence::setCameraZoom(uint16_t value) {
  value = constrain(value, m_limits.ZOOM_MIN, m_limits.ZOOM_MAX);
	return do_uint16_command(0x04, 0x47, value);
}

// Fetch the current value
bool VISCATelepresence::getCameraZoom(uint16_t& value) {
	 return do_uint16_query(0x04, 0x47, value);
}

// Zoom in the max
bool VISCATelepresence::setZoomInMax() {
	return do_uint8_command(0x04, 0x07, 0x2F);
}

// Zoom out to the max
bool VISCATelepresence::setZoomOutMax() {
	return do_uint8_command(0x04, 0x07, 0x3F);
}

// Control auto-focus
bool VISCATelepresence::setAutoFocus(bool enabled) {
	return do_uint8_command(0x04, 0x38, enabled?0x02:0x03);
}

// Fetch the current focus position
bool VISCATelepresence::getFocusValue(bool& autoFocus, uint16_t& value) {
	uint8_t tmp;
	if (!do_uint8_query(0x04, 0x38, tmp)) return false;
	autoFocus = tmp == 2;
	return do_uint16_query(0x04, 0x48, value);
}

// Set the manual focus position 64 to 574, range=510
bool VISCATelepresence::setFocusValue(uint16_t value) {
  value = constrain(value,m_limits.FOCUS_NEAR,m_limits.FOCUS_FAR);
	return do_uint16_command(0x04, 0x48, value);
}

// Zoom in the max
bool VISCATelepresence::setFarFocus() {
	return do_uint8_command(0x04, 0x08, 0x2F);
}

// Zoom out to the max
bool VISCATelepresence::setNearFocus() {
	return do_uint8_command(0x04, 0x08, 0x3F);
}

// Set the backlight compensation mode
bool VISCATelepresence::setBacklightCompensationMode(bool enabled) {
	return do_uint8_command(0x04, 0x33, enabled?2:3);
}

// Get the backlight compensation mode
bool VISCATelepresence::getBacklightCompensationMode(bool& enabled) {
	uint8_t tmp;
	if (!do_uint8_query(0x04, 0x33, tmp)) return false;
  enabled = tmp == 2;
	return true;
}

// Set auto white balance
bool VISCATelepresence::setAutoWhiteBalance(bool autoWBMode) {
	return do_uint8_command(0x04, 0x35, autoWBMode?0:6);
}

// Value is from 1 (yellow) to 16 (blue)
bool VISCATelepresence::setWhiteBalance(uint16_t value) {
  value = constrain(value, m_limits.WB_YELLOW, m_limits.WB_BLUE);
	return do_uint16_command(0x04, 0x75, value);
}

// Get white balance settings
bool VISCATelepresence::getWhiteBalanceValue(bool& automatic, uint16_t& value) {
	uint8_t tmp;
	if (!do_uint8_query(0x04, 0x35, tmp)) return false;
	automatic = tmp == 0;	
	return do_uint16_query(0x04, 0x75, value);
}

// Set auto exposure - when off the picture darkens, but no further controls do anything
bool VISCATelepresence::setAutoExposure(bool autoExposure) {
	return do_uint8_command(0x04, 0x39, autoExposure?0:0x3);
}

// Get the auto exposure mode
bool VISCATelepresence::getAutoExposure(bool& autoExposure) {
	uint8_t tmp;
	if (!do_uint8_query(0x04, 0x39, tmp)) return false;
	autoExposure = tmp == 0;
	return true;
}

// Sets the iris size (0-50) - doesnt appear to do anything
bool VISCATelepresence::setIrisValue(uint16_t value) {
  value = constrain(value, m_limits.IRIS_MIN, m_limits.IRIS_MAX);
	if (do_uint16_command(0x04, 0x4B, value)) {
		m_lastSetValues.irisValue = value;
		return true;
	}
	return false;
}

// Sets the picture gain (12-21) - doesnt appear to do anything
bool VISCATelepresence::setGainValue(uint16_t value) {	
   value = constrain(value, m_limits.GAIN_MIN, m_limits.GAIN_MAX);
	if (do_uint16_command(0x04, 0x4C, value)) {
		m_lastSetValues.gainValue = value;
		return true;
	}
	return false;
}

// Set Gamma Control
bool VISCATelepresence::setAutoGammaMode(bool autoGamma) {
	if (do_uint8_command(0x04, 0x51, autoGamma?2:3)) {
		m_lastSetValues.autoGamma = autoGamma;
		return true;
	}
	return false;
}

// Value is from 0 to 7
bool VISCATelepresence::setGammaValue(uint8_t value) {
	value = constrain(value, m_limits.GAMMA_MIN, m_limits.GAMMA_MAX);
	if (do_uint16_command(0x04, 0x52, value)) {
		m_lastSetValues.gammaValue = value;
		return true;
	}

	return false;
}

// Command doesnt work on camera so this has a fallback
bool VISCATelepresence::getGammaValue(bool& autoGamma, uint8_t& value) {
	uint16_t tmp2;
	uint8_t tmp;

	// Auto Gamma
	if (m_lastSetValues.autoGammaWorks && do_uint8_query(0x04, 0x51, tmp)) 
		m_lastSetValues.autoGamma = tmp == 2;
	else m_lastSetValues.autoGammaWorks = false;
	autoGamma = m_lastSetValues.autoGamma;

	// Gamma value
	if (m_lastSetValues.gammaValueWorks && do_uint16_query(0x04, 0x52, tmp2)) 
		m_lastSetValues.gammaValue = tmp2;
	else m_lastSetValues.gammaValueWorks = false;
	value = m_lastSetValues.gammaValue;
	return true;
}

// Get camera brightness value
bool VISCATelepresence::getBrightnessValue(uint8_t& value) {
	uint16_t tmp2;
	if (m_lastSetValues.brightnessValueWorks && do_uint16_query(0x04, 0x4D, tmp2)) {
  		value = tmp2;	
    return true;
  } else {
    m_lastSetValues.brightnessValueWorks  = false;
    value = m_lastSetValues.brightnessValue;
    return true;    
  }
}

// Sets the picture brightness (1-45) - I reverse engeineered this one
bool VISCATelepresence::setBrightnessValue(uint16_t value) {	
   value = constrain(value, m_limits.BRIGHTNESS_MIN, m_limits.BRIGHTNESS_MAX);
	 if (do_uint16_command(0x04, 0x4D, value)) {
      m_lastSetValues.brightnessValue = value;   
      return true;
   }
   return false;
}

// Command doesnt work on camera so this has a fallback
bool VISCATelepresence::getIrisValue(uint8_t& value) {
	uint16_t tmp2;
	if (m_lastSetValues.irisValueWorks && do_uint16_query(0x04, 0x4B, tmp2)) 
		m_lastSetValues.irisValue = tmp2;
	else m_lastSetValues.irisValueWorks = false;
	value = m_lastSetValues.irisValue;
	return true;
}

// Command doesnt work on camera so this has a fallback
bool VISCATelepresence::getGainValue(uint8_t& value) {
	uint16_t tmp2;
	if (m_lastSetValues.gainValueWorks && do_uint16_query(0x04, 0x4C, tmp2)) 
		m_lastSetValues.gainValue = tmp2; 
	else m_lastSetValues.gainValueWorks = false;
	
	value = m_lastSetValues.gainValue;
	return true;
}
