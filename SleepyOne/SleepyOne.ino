// 
// SleepyOne
//

//#define DEBUG

// **** INCLUDES *****
#include "SleepyPi2.h"
#include <Time.h>
#include <LowPower.h>
#include <PCF8523.h>
#include <Wire.h>

// States
typedef enum: byte {
  eWAIT,
  eBUTTON_PRESSED,
  eBUTTON_HELD,
  eBUTTON_RELEASED
} eBUTTONSTATE;

typedef enum: byte {
   PI_OFF,
   PI_BOOTING,
   PI_ON,
   PI_SHUTTING_DOWN
} ePISTATE;

// **** STATE *****
typedef enum: byte {
  WAKE_POWER,
  WAKE_ALARM,
  WAKE_BUTTON
} eWAKESTATE;

const char *monthName[12] = {
  "Jan", "Feb", "Mar", "Apr", "May", "Jun",
  "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
}; 

const byte LED_PIN = 13;
const byte ALARM_INTRPT_PIN = 0;
const byte BUTTON_INTRPT_PIN = 1;

// Globals
#define kBUTTON_POWEROFF_TIME_MS   2000
#define kBUTTON_FORCEOFF_TIME_MS   8000
byte WakeUp_StartHour       = 12;   // Hour in 24 hour clock
byte WakeUp_StartMinute     = 00;  // Minutes 
unsigned long MAX_RUNTIME = 1800 * 1000UL; // millis
unsigned long SHUTDOWN_KILL_DELAY = 120 * 1000UL; // millis

// Variables
//volatile bool buttonPressed = false;
tmElements_t tm;
ePISTATE piState = PI_OFF;
volatile eWAKESTATE wuState = WAKE_POWER;
unsigned long  piStartMillis, currentRunTime, shutdownCalledMillis, shutdownTime;
bool piRunning = false;
bool ledState = LOW;


// ISRs
void button_isr() {
  // A handler for the Button interrupt.
  //wuState = WAKE_BUTTON;
}

void alarm_isr() {
  // Just a handler for the alarm interrupt.
  //wuState = WAKE_ALARM;
}

// Functions
void startPi() {
  SleepyPi.enableExtPower(true);
  SleepyPi.enablePiPower(true);
  digitalWrite(LED_PIN, HIGH);
  piState = PI_BOOTING;
  piStartMillis = millis();
}

void shutdownPi() {
  SleepyPi.piShutdown();
  SleepyPi.enableExtPower(false);
  digitalWrite(LED_PIN, LOW);
  piState = PI_SHUTTING_DOWN;
  shutdownCalledMillis = millis();
}

void killPi() {
  SleepyPi.enablePiPower(false);
  SleepyPi.enableExtPower(false);
  digitalWrite(LED_PIN, LOW);
  piState = PI_OFF;
}

void checkRuntime() {
  currentRunTime = millis() - piStartMillis;
  if (currentRunTime > MAX_RUNTIME) {
    shutdownPi();
  }
}

void setup() {
  // Configure "Standard" LED pin
  pinMode(LED_PIN, OUTPUT);		
  
  // Make sure that were off on startup
  killPi();
  
  // initialize serial communication: In Arduino IDE use "Serial Monitor"
  delay(50);
  
  SleepyPi.rtcInit(true);
}

void loop() {  
    if (piState == PI_OFF) {
      // If pi is not running we want to setup alarms, interrupts and go to sleep
      SleepyPi.rtcClearInterrupts();
      
      // Set up button trigger
      attachInterrupt(BUTTON_INTRPT_PIN, button_isr, LOW);
      
      // Set up alarm
      attachInterrupt(ALARM_INTRPT_PIN, alarm_isr, FALLING); 
      SleepyPi.enableWakeupAlarm(true);
      SleepyPi.setAlarm(WakeUp_StartHour,WakeUp_StartMinute);
      
      delay(500);
      
      // Sleep
      SleepyPi.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);
      
      // ZZZ... now we are sleeping
      digitalWrite(LED_PIN, HIGH);
      // Woken from slumber, disable interrupts   
      detachInterrupt(BUTTON_INTRPT_PIN);
      detachInterrupt(ALARM_INTRPT_PIN);
      SleepyPi.ackAlarm();
      
      startPi();
    } else {
      piRunning = SleepyPi.checkPiStatus(false); 
      //piRunning = true; 
        // If pi is in power on state, but did shutdown by itself, we can kill power
      if (piRunning == false && piState != PI_BOOTING) { 
        killPi();
      } else {
        switch (piState) {
          case PI_SHUTTING_DOWN:
            ledState = !ledState;
            digitalWrite(LED_PIN, ledState);
            shutdownTime = millis() - shutdownCalledMillis;
            if (shutdownTime > SHUTDOWN_KILL_DELAY) {
              killPi();
            }
            break;
          case PI_BOOTING:
            if (piRunning == true) {
              // We have booted up!
              piState = PI_ON;         
            }
            checkRuntime();
            break;
          case PI_ON: // intentional drop thru
          default:
            checkRuntime();
            break;
        }
      }
      // wait for half a second to lower consumption, animate LED blink
      delay(500);
    }
}
