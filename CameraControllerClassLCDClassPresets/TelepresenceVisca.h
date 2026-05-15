#ifndef TELEPRESENCE_VISCA_H
#define TELEPRESENCE_VISCA_H

#include <SoftwareSerial.h>

// If enabled, all visca messages sent and received will be sent over the arduino serial port
//#define ENABLE_VISCA_DEBUG_MESSAGES 

// The format used to send/receive a VISCA command
#define VISCACOMMAND_MAX_LENGTH 64

class VISCATelepresence {
  public:
  
    struct Limits {
      uint16_t PAN_MIN;
      uint16_t PAN_MAX;
      uint16_t TILT_MIN;
      uint16_t TILT_MAX;
      uint16_t ZOOM_MIN;
      uint16_t ZOOM_MAX;
      uint16_t FOCUS_NEAR;
      uint16_t FOCUS_FAR;
      uint16_t WB_YELLOW;
      uint16_t WB_BLUE;
      uint16_t PAN_MIDDLE;
      uint16_t TILT_MIDDLE;
      uint8_t BRIGHTNESS_MIN;
      uint8_t BRIGHTNESS_MAX;
    };  
    
	private:
		// Software serial port for the camera
		SoftwareSerial m_serialPort;
		
		// Camera address 
		uint8_t m_cameraAddress = 1;

		// Visca command structure
		struct VISCACommand {
		  uint8_t len;
		  uint8_t payload[VISCACOMMAND_MAX_LENGTH];
		};
		
		// Not all the commands seem to work properly on teleprence, so this covers that up
		// To hold values we can write, but cant read back for some reason
		struct {
		  // Values that cant be saved
      bool brightnessValueWorks = true;
      uint8_t brightnessValue;
		} m_lastSetValues;

    // Camera limits
    Limits m_limits;
				
		// Sends a visca command, and if successful the response is spat back out in the command 
		bool exchangeCommand(VISCACommand& command, unsigned long timeout = 1000);
				
		// Runs a command that sets a single byte value (eg: 8x 01 cl cmdl value ff)
		bool do_uint8_command(uint8_t cl, uint8_t cmdl, uint8_t value);

		// Runs a query that fetches 8-bit value (eg: 8x 09 cl cmdl ff -- with response being 8x 50 output FF) 
		bool do_uint8_query(uint8_t cl, uint8_t cmdl, uint8_t& output);
		  
		// Runs a command that sets a 16-bit value (eg: 8x 01 cl cmdl valueA valueB valueC valueD ff) 
		bool do_uint16_command(uint8_t cl, uint8_t cmdl, uint16_t value);

		// Runs a query that fetches 16-bit value (eg: 8x 09 cl cmdl ff -- with response being 8x 50 outputA outputB outputC outputD FF) 
		bool do_uint16_query(uint8_t cl, uint8_t cmdl, uint16_t& output);

		// Set the camera to be on a specific address
		bool setCameraAddress(uint8_t address);	

    // Yeah a little nasty
    void setLimits(uint16_t PAN_MIN,uint16_t PAN_MAX,uint16_t PAN_MIDDLE,uint16_t TILT_MIN,uint16_t TILT_MAX,uint16_t TILT_MIDDLE,uint16_t ZOOM_MIN,uint16_t ZOOM_MAX,uint16_t FOCUS_NEAR,uint16_t FOCUS_FAR,
               uint16_t WB_YELLOW,uint16_t WB_BLUE, uint8_t BRIGHTNESS_MIN, uint8_t BRIGHTNESS_MAX);


	public:
    enum CameraType {ctUnknown, ct1080p4xS2,ct1080p12xS2};  

		// Constructor
		VISCATelepresence(uint8_t CameraRXPin, uint8_t cameraTXPin);

    // Get camera limits
    const Limits* getCameraLimits();
    
    // Attempt to identify the camera serial number
    bool getCameraSerialNumber(char* serial, uint8_t maxLength);

    // Get software (dont send to a sony camera)
    bool getCameraSoftwareVersion(char* softwareVersion, uint8_t maxLength);

    // Changes the limits on the functions. ctUnknown removes limits 
    void setCameraMode(CameraType mode);
    		
		// Initialises the camera, optionally resetting it, and ensures all default values have been provided
		bool initialise(uint8_t cameraAddress, bool performReset);
		
		// Sends a camera reboot message
		bool resetCamera();

		// Enable/Disables the IR TX/RX
		bool enableIR(bool enable);

		// Control picture flip
		bool setHVFlip(bool hFlip, bool vFlip);
	
		// Get the status of H and V flip
		bool getHVFlip(bool& hFlip, bool& vFlip);

		// Change the call led
		bool setCallLED(bool enabled);

		// Set the current Pan and Tilt position
		bool setPanTilt(uint16_t pan, uint16_t tilt);

		// Set the current Pan and Tilt position
		bool getPanTilt(uint16_t& pan, uint16_t& tilt);

		// Set the pan/tilt speed
		bool setPanTiltSpeed(int16_t panSpeed, int16_t tiltSpeed, bool& moving);

		// STOP movement
		bool panTiltStop();
		
		// Resets the pan/tilt to center, which also updates motor sync
		bool resetPanTilt();

		// Change the current zoom level.  Range is 4096 to 11296, range is 7200
		bool setCameraZoom(uint16_t value);

		// Fetch the current value
		bool getCameraZoom(uint16_t& value);

		// Zoom in the max
		bool setZoomInMax();

		// Zoom out to the max
		bool setZoomOutMax();

		// Control auto-focus
		bool setAutoFocus(bool enabled);

		// Fetch the current focus position
		bool getFocusValue(bool& autoFocus, uint16_t& value);

		// Set the manual focus position 64 to 574, range=510
		bool setFocusValue(uint16_t value);

		// Zoom in the max
		bool setFarFocus();

		// Zoom out to the max
		bool setNearFocus();

		// Set auto white balance
		bool setAutoWhiteBalance(bool autoWBMode);

		// Value is from 1 (yellow) to 16 (blue)
		bool setWhiteBalance(uint16_t value);

		// Get white balance settings
		bool getWhiteBalanceValue(bool& automatic, uint16_t& value);

		// Set auto exposure - when off the picture darkens, but no further controls do anything
		bool setAutoExposure(bool autoExposure);

		// Get the auto exposure mode
		bool getAutoExposure(bool& autoExposure);

    // Get camera brightness value
    bool getBrightnessValue(uint8_t& value);

    // Sets the picture brightness (1-45) - I reverse engeineered this one
    bool setBrightnessValue(uint16_t value);

    // Special command that sets all four things in one go
    bool setPanTiltZoomFocus(uint16_t pan, uint16_t tilt, uint16_t zoom, uint16_t focus);
};




#endif
