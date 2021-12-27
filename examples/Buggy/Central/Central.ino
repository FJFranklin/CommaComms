/* -*- mode: c++ -*-
 * 
 * Copyright 2020-21 Francis James Franklin
 * 
 * Open Source under the MIT License - see LICENSE in the project's root folder
 */

#include "config.hh"
#include "Timer.hh"
#include "CC_Serial.hh"
#include "Encoders.hh"

#ifdef ENABLE_GPS
#include <Adafruit_GPS.h>
#endif

#include "Claw.hh"

class Central : public Timer, public CommaComms::CC_Responder {
private:
  CC_Serial s0;
  CC_Serial s2;

#ifdef ENABLE_GPS
  Adafruit_GPS *gps;
#endif

  elapsedMicros report;
  unsigned char reportMode;
  bool bReportGenerated;

public:
  Central() :
    s0(Serial, '0', this),
    s2(Serial2, '2', this),
#ifdef ENABLE_GPS
    gps(new Adafruit_GPS(&Serial3)),
#endif
    report(0),
    reportMode(0),
    bReportGenerated(false)
  {
#ifdef ENABLE_GPS
    gps->begin(9600);
    gps->sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA);
    gps->sendCommand(PMTK_API_SET_FIX_CTL_5HZ);
    gps->sendCommand(PMTK_SET_NMEA_UPDATE_5HZ);
  //gps->sendCommand(PMTK_SET_NMEA_UPDATE_1HZ);
  //gps->sendCommand(PGCMD_ANTENNA);
    delay(1000);
#endif
  }
  virtual ~Central() {
    // ...
  }
  bool reporting() {
    return (reportMode == 1);
  }

  virtual void notify(CommaComms * C, const char * str) {
    if (Serial) {
      Serial.print(C->name());
      Serial.print(": notify: ");
      Serial.println(str);
    }
  }
  virtual void command(CommaComms * C, char code, unsigned long value) {
#ifdef ENABLE_FEEDBACK
    if (Serial) {
      Serial.print(C->name());
      Serial.print(": command: ");
      Serial.print(code);
      Serial.print(": ");
      Serial.println(value);
    }
#endif

    switch(code) {
    case 'f':
      MSpeed = (value > 127 ? 127 : value);
      TB_Params.target = (float) MSpeed / 10.0;
      break;
    case 'b':
      MSpeed = (value > 127 ? -127 : -value);
      TB_Params.target = (float) MSpeed / 10.0;
      break;
    case 'x':
      MSpeed = 0;
      TB_Params.target = 0.0;
      break;
    case 'l':
      M2_enable = value ? true : false;
      break;
    case 'r':
      M1_enable = value ? true : false;
      break;
    case 'c':
      if (value == 1) s_roboclaw_init(); // for testing only
      break;
    case 'S':
      TB_Params.slip = (float) value / 100.0;
      break;
    case 'P':
      MC_Params.P = (float) value / 100.0;
      break;
    case 'I':
      MC_Params.I = (float) value / 100.0;
      break;
    case 'D':
      MC_Params.D = (float) value / 100.0;
      break;
    case 'Q':
      TB_Params.P = (float) value / 100.0;
      break;
    case 'J':
      TB_Params.I = (float) value / 100.0;
      break;
    case 'E':
      TB_Params.D = (float) value / 100.0;
      break;
    case 'R':
      reportMode = (unsigned char) (value & 0xFF); // 0 for none; 1 for GPS-triggered reporting; 2 for buggy 'actual' at 10ms intervalsb
      break;
    default:
      break;
    }
  }

  virtual void every_milli() { // runs once a millisecond, on average
    // ...
  }

  virtual void every_10ms() { // runs once every 10ms, on average
#ifndef ENABLE_PID
    /* Default behaviour: No PID control, just a linear smoothing of speed transition
     */
    int M1 = M1_actual;

    if (M1 < MSpeed) {
      ++M1;
    }
    if (M1 > MSpeed) {
      --M1;
    }
    s_roboclaw_set_M1(M1); // right

    int M2 = M2_actual;

    if (M2 < MSpeed) {
      ++M2;
    }
    if (M2 > MSpeed) {
      --M2;
    }
    s_roboclaw_set_M2(M2); // left

    if ((reportMode == 3) && (MSpeed || M1_actual || M2_actual)) {
      char buf[40];
      snprintf(buf, 40, "%d %d/%d %d/%d", MSpeed, M1, M1_actual, M2, M2_actual);
      s0.ui_print(buf);
      s0.ui();
    }
#endif // ! ENABLE_PID

    E1.sync();
    E2.sync();
    E3.sync();
    E4.sync();

    const float scaling = PI * WHEEL_DIAMETER * 3.6;

    TB_Params.actual_FL = E1.latest() * scaling; // * * * FIXME - which encoders correspond to which wheels? * * *
    TB_Params.actual_BL = E2.latest() * scaling; // Note: Important: Check! Is +ve forwards on the left and backwards on the right?
    TB_Params.actual_FR = E3.latest() * scaling; // Also FIXME: This should be the only place .latest() is used. Tidy up instances below.
    TB_Params.actual_BR = E4.latest() * scaling;

    TB_Params.actual = (TB_Params.actual_FL - TB_Params.actual_BR) / 2.0;

    if ((reportMode == 2) && (TB_Params.actual_FL || TB_Params.actual_BL || TB_Params.actual_FR || TB_Params.actual_BR || TB_Params.actual)) {
      char buf[40];
      snprintf(buf, 40, "%6.2f %6.2f %6.2f %6.2f %6.2f", TB_Params.actual_FL, TB_Params.actual_BL, TB_Params.actual_FR, TB_Params.actual_BR, TB_Params.actual);
      s0.ui_print(buf);
      s0.ui();
    }
#ifdef ENABLE_PID
    s_buggy_update(10); // see Claw.hh
#endif // ENABLE_PID
  }

  virtual void every_tenth(int tenth) { // runs once every tenth of a second, where tenth = 0..9
    if (tenth == 4) {
      // If actually generating output data (the GPS needs to be active for this if in mode 1) add an extra blink; should be dash-dot-dash-dot
      digitalWrite(LED_BUILTIN, bReportGenerated || (reportMode == 2));
      bReportGenerated = false;
    } else {
      digitalWrite(LED_BUILTIN, tenth == 0 || tenth == 8 || (reporting() && tenth == 9)); // double blink per second, or long single if in reporting mode 1
    }

    if (tenth == 0 || tenth == 5) { // i.e., every half-second
      bool moving = false;

      float vs1 = TB_Params.actual_FL; // Vehicle speed in km/h
      float vs2 = TB_Params.actual_BL;
      float vs3 = TB_Params.actual_FR;
      float vs4 = TB_Params.actual_BR;
      moving = vs1 || vs2 || vs3 || vs4;

      if (moving || MSpeed || M1_actual || M2_actual) {
        char str[64];
        snprintf(str, 64, "M: %d {%d %d} v: %.2f %.2f %.2f %.2f km/h", MSpeed, M1_actual, M2_actual, vs1, vs2, vs3, vs4);
#ifndef ENABLE_GPS
        if (Serial) {
          Serial.println(str);
        }
#endif
        s2.command_print(str);
      }
    }
  }

  virtual void every_second() { // runs once every second
    // ...
  }

  void generate_report() {
    char buf[48];

#ifdef ENABLE_GPS
    snprintf(buf, 48, "%02d/%02d/20%02d,%02d.%02d,%02d.%04u,",
      (int) gps->day,
      (int) gps->month,
      (int) gps->year,
      (int) gps->hour,
      (int) gps->minute,
      (int) gps->seconds,
      (unsigned int) gps->milliseconds);
    s0.ui_print(buf);

    if (gps->fix) {
      float coord = abs(gps->latitudeDegrees);
      int degrees = (int) coord;
      coord = (coord - (float) degrees) * 60;
      int minutes = (int) coord;
      coord = (coord - (float) minutes) * 60;
            
      snprintf(buf, 48, "%3d^%02d'%.4f\"%c,", degrees, minutes, coord, gps->lat ? gps->lat : ((gps->latitudeDegrees < 0) ? 'S' : 'N'));
      s0.ui_print(buf);
            
      coord = abs(gps->longitudeDegrees);
      degrees = (int) coord;
      coord = (coord - (float) degrees) * 60;
      minutes = (int) coord;
      coord = (coord - (float) minutes) * 60;
            
      snprintf(buf, 48, "%3d^%02d'%.4f\"%c,", degrees, minutes, coord, gps->lon ? gps->lon : ((gps->longitudeDegrees < 0) ? 'W' : 'E'));
      s0.ui_print(buf);

      snprintf(buf, 48, "%.6f,%.6f,", gps->latitudeDegrees, gps->longitudeDegrees);
      s0.ui_print(buf);
    } else {
      s0.ui_print(",,,,");
    }
#endif

    snprintf(buf, 48, "%10lu", (unsigned long) millis());
    s0.ui_print(buf);

    snprintf(buf, 48, ",%3d,%3d,%3d,", MSpeed, M1_actual, M2_actual);
    s0.ui_print(buf);

    float vs1 = TB_Params.actual_FL; // Vehicle speed in km/h
    float vs2 = TB_Params.actual_BL;
    float vs3 = TB_Params.actual_FR;
    float vs4 = TB_Params.actual_BR;

    snprintf(buf, 48, "%.3f,%.3f,%.3f,%.3f", vs1, vs2, vs3, vs4);
    s0.ui_print(buf);
    s0.ui();

    bReportGenerated = true;
  }

  virtual void tick() {
    s0.update(); // important: housekeeping
    s2.update(); // important: housekeeping
#ifdef ENABLE_GPS
    if (gps->available()) {
      gps->read();
      if (gps->newNMEAreceived()) {
        if (gps->parse(gps->lastNMEA())) {
          if (reporting() && report > 100000) {
            report = 0;
            generate_report();
          }
        }
      }
    }
#endif
  }
};

void setup() {
  while (millis() < 500) {
    if (Serial) break;
  }
  Serial.begin(115200);
  while (millis() < 500) {
    if (Serial2) break;
  }
  Serial2.begin(115200);

  s_roboclaw_init(); // Setup RoboClaw

  E1.init(E1_ChA, E1_ChB, ENCODER_PPR, false); // set up encoder 1
  E2.init(E2_ChA, E2_ChB, ENCODER_PPR, false); // set up encoder 2
  E3.init(E3_ChA, E3_ChB, ENCODER_PPR, false); // set up encoder 3
  E4.init(E4_ChA, E4_ChB, ENCODER_PPR, false); // set up encoder 4

  pinMode(LED_BUILTIN, OUTPUT);

  Central().run();
}

void loop() {
  // ...
}
