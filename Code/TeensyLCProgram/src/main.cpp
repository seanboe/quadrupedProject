#include <Arduino.h>

//graphics libraries, for tft display
#include <Adafruit_GFX.h>    
#include <Adafruit_ST7735.h>

// definitions for SPI pins for display
#define TFT_CS        6
#define TFT_RST       2
#define TFT_DC        3

//usb host libraries for interfacing with ps3 controller
#include <PS3BT.h>
#include <usbhub.h>

#ifdef dobogusinclude
#include <spi4teensy3.h>
#endif

// SPI: for tft display and usb host libraries, Wire: for coms with teensy 4.1
#include <SPI.h>
#include <Wire.h>

// display object initlizaation, as shown in the example code; USES SPI1
Adafruit_ST7735 tft = Adafruit_ST7735(&SPI1, TFT_CS, TFT_DC, TFT_RST);

// usb object initialization, as shown in the example code
USB Usb;
BTD Btd(&Usb); 
PS3BT PS3(&Btd, 0x5C, 0xF3, 0x70, 0x9D, 0x84, 0x81);  // actual bluetooth address from dongle - set ENABLE_UHS_DEBUGGING to 1 in settings.h

#define SLAVE_ADDRESS  8    // this device's address, for I2C

#define I2CBYTES  17

// the array that stores controller data; could be a struct instead, but is read as an array by the CoprocessorTalk library so better for continuity.
// Array indexes are all defined there as well.
volatile uint8_t array[I2CBYTES] = {0};

// robot mode, received from a ping from the teensy 4.1 and shown on the display
volatile uint8_t mode;
volatile bool updateModeFlag = false;          // flag for when mode data is received from teensy 4.1, used in slaveReceiveHandler();

// function forward declarations
void masterRequestHandler();
void slaveReceiveHandler(int incomingBytes);

void displayControllerStatus();
void updateStats();

void setup() {
  Serial.begin(9600);

  // usb library initialization and dongle intialization
  if (Usb.Init() == -1) {
    Serial.print(F("\r\nOSC did not start"));
    while (1); //halt
  }
  Serial.print(F("\r\nPS3 Bluetooth Library Started"));

  // join the I2C bus with the teensy 4.1 and set request handler
  Wire.begin(SLAVE_ADDRESS);
  Wire.onRequest(masterRequestHandler);
  Wire.onReceive(slaveReceiveHandler);

  // initialize the display and setup permanent text 
  tft.initR(INITR_BLACKTAB);      // Init ST7735S chip, black tab
  tft.setRotation(1);             // rotate the screen to "landscape"
  tft.fillScreen(ST7735_BLACK);
  tft.setCursor(10, 10);
  tft.print("PS3 connected: ");
  tft.setCursor(10, 25);
  tft.print("Mode: ");
}
void loop() {
  Usb.Task();

  array[0] = (PS3.PS3Connected ? 1 : 0);

  if (PS3.PS3Connected) {
      
    if (PS3.getButtonClick(PS))
      PS3.disconnect();

    // joysticks
    array[1] = PS3.getAnalogHat(LeftHatX);
    array[2] = PS3.getAnalogHat(LeftHatY);
    array[3] = PS3.getAnalogHat(RightHatX);
    array[4] = PS3.getAnalogHat(RightHatY);

    // L1, L2, R1, R2
    array[5] = (PS3.getButtonPress(L1) ? 1 : 0);
    array[6] = PS3.getAnalogButton(L2);
    array[7] = (PS3.getButtonPress(R1) ? 1 : 0);
    array[8] = PS3.getAnalogButton(R2);

    // D-pad buttons
    array[9] = (PS3.getButtonPress(DOWN) ? 1 : 0);
    array[10] = (PS3.getButtonPress(LEFT) ? 1 : 0);
    array[11] = (PS3.getButtonPress(UP) ? 1 : 0);
    array[12] = (PS3.getButtonPress(RIGHT) ? 1 : 0);

    // action buttons (cross, square, triangle, circle)
    array[13] = (PS3.getButtonPress(CROSS) ? 1 : 0);
    array[14] = (PS3.getButtonPress(SQUARE) ? 1 : 0);
    array[15] = (PS3.getButtonPress(TRIANGLE) ? 1 : 0);
    array[16] = (PS3.getButtonPress(CIRCLE) ? 1 : 0);

    /* this is test code to check if stuff is being read correctly*/
    // for (int x = 0; x < I2CBYTES; x++) {
    //   Serial.print(array[x]);
    // }
    // Serial.println();

  }
  
  displayControllerStatus();
  updateStats();
  
}


void masterRequestHandler() {
  Wire.write((const uint8_t *) array, I2CBYTES);
}


/* In the future, I may want to receive status data from the main processor, like numbers
(for certain control modes) or simple error messages. Different data requires a different 
variable type, obviously, so first my function will expect a number (pingType) to know what
will be received. Depending on this, I can read the following data into different variables. 
For now, I'll only to the mode because this is simple and a good example (I also don't have 
any error messages because I haven't written anything yet :/ ). To add more, please declare a new
global variable (must be global and volatile for the interrupt) and add the functionality in 
void updateStats. This will update everything (a new variable requires a new pointer to be added) 
depending on whether the updateModeFlag is true, which will be set true when anything is received
from the teensy 4.1. The pingType [s] should be #defines in the Coprocessor.h library accessible
to the teensy 4.1*/
void slaveReceiveHandler(int incomingBytes) {
  int pingType = Wire.read()   ;
  if (pingType == 0) {
    mode = Wire.read();               // mode is one number
  }
  updateModeFlag = true;
}


void displayControllerStatus() {
  static int previousConnectionStatus = 1;        // set to 1, so that this function is run once when controller isn't connected yet
  
  if ((millis() % 500 == 0) && previousConnectionStatus != array[0]) {
    
    tft.setCursor(100, 10);
    tft.fillRect(100, 10, 35, 7, ST7735_BLACK);
    previousConnectionStatus = array[0];
    
    if (array[0] == 1) {
      tft.setTextColor(ST7735_GREEN);
      tft.print("True");
    }
    else if (array[0] == 0) {
      tft.setTextColor(ST7735_RED);
      tft.print("False");
    }
    
  }
}


// updates stats on the display to whatever has been ping-ed from teensy 4.1; no pointers because all the variables are volatile anyway (volatile has no pass-by-reference)
void updateStats() {
  if (updateModeFlag) {
    tft.setCursor(50,26);
    tft.setTextColor(ST7735_BLUE);
    tft.fillRect(50, 26, 15, 7, ST7735_BLACK);
    tft.print(mode);
  }
  updateModeFlag = false;
}

