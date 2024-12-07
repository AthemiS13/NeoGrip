/*
 * LucidGloves Firmware Version 4
 * Author: Lucas_VRTech - LucidVR
 * lucidvrtech.com
 */

#include "AdvancedConfig.h"

//This is the configuration file, main structure in _main.ino
//CONFIGURATION SETTINGS:
#define COMMUNICATION COMM_SERIAL //Which communication protocol to use
//serial over USB
  #define SERIAL_BAUD_RATE 115200
  
//serial over Bluetooth
  #define BTSERIAL_DEVICE_NAME "lucidgloves-left"

//ANALOG INPUT CONFIG
#define FLIP_POTS  false  //Flip values from potentiometers (for fingers!) if they are backwards

//Gesture enables, make false to use button override
#define TRIGGER_GESTURE false
#define GRAB_GESTURE    false
#define PINCH_GESTURE   false


//BUTTON INVERT
//If a button registers as pressed when not and vice versa (eg. using normally-closed switches),
//you can invert their behaviour here by setting their line to true.
//If unsure, set to false
#define INVERT_A false
#define INVERT_B false
#define INVERT_JOY false
#define INVERT_MENU false
#define INVERT_CALIB false
//These only apply with gesture button override:
#define INVERT_TRIGGER false
#define INVERT_GRAB false
#define INVERT_PINCH false


//joystick configuration
#define JOYSTICK_BLANK true //make true if not using the joystick
#define JOY_FLIP_X false
#define JOY_FLIP_Y false
#define JOYSTICK_DEADZONE 20 //deadzone in the joystick to prevent drift (in percent)

#define NO_THUMB false //If for some reason you don't want to track the thumb

#define USING_CALIB_PIN false//When PIN_CALIB is shorted (or it's button pushed) it will reset calibration if this is on.

#define USING_FORCE_FEEDBACK false //Force feedback haptics allow you to feel the solid objects you hold
#define SERVO_SCALING false //dynamic scaling of servo motors

#if defined(ESP32)
  //(This configuration is for ESP32 DOIT V1 so make sure to change if you're on another board)


  //LEFT CONTROLLER CONFIG
  #define PIN_JOY_X     20
  #define PIN_JOY_Y     20
  #define PIN_JOY_BTN   20
  #define PIN_A_BTN     27
  #define PIN_B_BTN     26
  #define PIN_TRIG_BTN  12 //unused if gesture set
  #define PIN_GRAB_BTN  14 //unused if gesture set
  #define PIN_PNCH_BTN  20//unused if gesture set
  #define PIN_CALIB     20 //button for recalibration (You can set this to GPIO0 to use the BOOT button, but only when using Bluetooth.)
  #define PIN_MENU_BTN  20

  //UNUSED
  #define PIN_PINKY     20
  #define PIN_RING      20
  #define PIN_MIDDLE    20
  #define PIN_INDEX     20
  #define PIN_THUMB     20
  #define DEBUG_LED 20
  #define PIN_PINKY_MOTOR     20  //used for force feedback
  #define PIN_RING_MOTOR      20//^
  #define PIN_MIDDLE_MOTOR    20 //^
  #define PIN_INDEX_MOTOR     20  //^
  #define PIN_THUMB_MOTOR     20 //^

  
//PINS CONFIGURATION 
#elif defined(__AVR__)
  //(This configuration is for Arduino Nano so make sure to change if you're on another board)
  #define PIN_PINKY     A0
  #define PIN_RING      A1
  #define PIN_MIDDLE    A2
  #define PIN_INDEX     A3
  #define PIN_THUMB     A4
  #define PIN_JOY_X     A6
  #define PIN_JOY_Y     A7
  #define PIN_JOY_BTN   7 
  #define PIN_A_BTN     8 
  #define PIN_B_BTN     9
  #define PIN_TRIG_BTN  10 //unused if gesture set
  #define PIN_GRAB_BTN  11 //unused if gesture set
  #define PIN_PNCH_BTN  12 //unused if gesture set
  #define PIN_CALIB     13 //button for recalibration
  #define DEBUG_LED     LED_BUILTIN
  #define PIN_PINKY_MOTOR     2 //used for force feedback
  #define PIN_RING_MOTOR      3 //^
  #define PIN_MIDDLE_MOTOR    4 //^
  #define PIN_INDEX_MOTOR     5 //^
  #define PIN_THUMB_MOTOR     6 //^
  #define PIN_MENU_BTN        8
#endif