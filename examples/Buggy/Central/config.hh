/* Copyright 2020-22 Francis James Franklin
 * 
 * Open Source under the MIT License - see LICENSE in the project's root folder
 */

#ifndef cariot_config_hh
#define cariot_config_hh

#include <Arduino.h>

//#define USE_SERIAL_4      // output data to Serial4 instead of Serial
//#define USE_SERIAL_5      // enable shell on Serial5

#define ENABLE_ROBOCLAW   // motor control using RoboClaw; comment to disable
#define ENABLE_PID        // PID motor control - experimental
#define ENABLE_GPS        // required for GPS; comment to disable
//#define ENABLE_FEEDBACK   // echo received commands to Serial, if available // FIXME - collisions!

#define TARGET_TRACKBUGGY // Control circuit for the Track Buggy
//#define TARGET_PROTOTYPE  // Control circuit for the prototype

#define APP_MOTORCONTROL  // Command channel on Serial2; RoboClaw on Serial1

#if defined(TARGET_TRACKBUGGY)
#define ENCODER_PPR    1024    // encoder resolution
#define WHEEL_DIAMETER    0.12 // TODO - check!!
#endif
#if defined(TARGET_PROTOTYPE)
#define ENCODER_PPR     256    // encoder resolution
#define WHEEL_DIAMETER    0.16
#endif

/* Pins for encoders, if using the Encoder class
 */
#define E1_ChA 14 // yellow is /A
#define E1_ChB 15 // pink   is /B
#define E2_ChA 16 // yellow is /A
#define E2_ChB 17 // pink   is /B
#define E3_ChA 18 // yellow is /A
#define E3_ChB 19 // pink   is /B
#define E4_ChA 20 // yellow is /A
#define E4_ChB 21 // pink   is /B

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
