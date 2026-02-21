/*
 * *****************************************************************************
 * CLIMATIC CHAMBER
 * *****************************************************************************
 * Closed-loop control of temperature, relative humidity, and CO₂ levels
 * with fresh air management, implemented on an Arduino Portenta Machine
 * Control platform.
 * *****************************************************************************
 */

// GLOBAL VARIABLES ------------------------------------------------------------
unsigned long serial_prev_ms = 0;
const unsigned long serial_interval_ms = 1000;

// INCLUDES --------------------------------------------------------------------
#include <Arduino.h>
#include <Arduino_PortentaMachineControl.h>

// FUNCTIONS *******************************************************************
// put function declarations here:
int myFunction(int, int);

// SETUP ***********************************************************************
void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
}

// LOOP ************************************************************************
void loop() {
  // put your main code here, to run repeatedly:
  unsigned long now = millis();
  if (now - serial_prev_ms >= serial_interval_ms) {
    serial_prev_ms = now;
    Serial.println("hello!");
  }
}

// FUNCTION DEFINITIONS -------------------------------------------------------