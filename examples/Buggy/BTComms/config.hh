/* Copyright 2020-21 Francis James Franklin
 * 
 * Open Source under the MIT License - see LICENSE in the project's root folder
 */

#ifndef cariot_config_hh
#define cariot_config_hh

#include <Arduino.h>

//#define ENABLE_FEEDBACK   // echo received commands to Serial, if available // FIXME - collisions!

/* Currently Bluetooth only work for the Feather M0
 */
#if defined(ADAFRUIT_FEATHER_M0)
#define FEATHER_M0_BTLE
#endif

#if !defined(TEENSYDUINO)
/* elapsedMicros is defined automatically for Teensy; need to define it for Arduino - this is a partial definition only:
 */
class elapsedMicros {
private:
  unsigned long m_micros;

public:
  elapsedMicros(unsigned long us = 0) {
    m_micros = micros() - us;
  }
  ~elapsedMicros() {
    //
  }
  operator unsigned long () const {
    return micros() - m_micros;
  }
  elapsedMicros & operator = (unsigned long us) {
    m_micros = micros() - us;
    return *this;
  }
};

#endif

#endif /* !cariot_config_hh */
