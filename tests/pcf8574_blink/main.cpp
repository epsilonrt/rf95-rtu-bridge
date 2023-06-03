// PCF8574 Blink

// Turns an LED on for one second, then off for one second, repeatedly.

// Created 13 Febrary 2021

// This example code is in the public domain.
#ifdef __unix__
#include <Piduino.h>  // All the magic is here ;-)
#else
// Defines the serial port as the console on the Arduino platform
#define Console Serial
#endif

#include "pcf8574.h"

Pcf8574 pcf8574;

const int Led = P0;

void setup() {

  Console.println ("PCF8574 Blink");
  
  pcf8574.pinMode (Led, OUTPUT);
  pcf8574.digitalWrite (Led, HIGH); // P0 connected to the led cathode
  
  Wire.begin (2);
  if (! pcf8574.begin()) {

    Console.println ("Unable to connect to PCF8574, check wiring, slave address and bus id !");
    exit (EXIT_FAILURE);
  }
}

void loop () {

  // Press Ctrl+C to abort ...
  pcf8574.digitalWrite (Led, LOW);
  delay (1000);         // wait for a second
  pcf8574.digitalWrite (Led, HIGH);
  delay (1000);         // wait for a second
}
