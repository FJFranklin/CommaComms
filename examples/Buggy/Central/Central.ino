/* -*- mode: c++ -*-
 * 
 * Copyright 2020-22 Francis James Franklin
 * 
 * Open Source under the MIT License - see LICENSE in the project's root folder
 */

#include "config.hh"
#include "MultiShell.hh"
#include "Timer.hh"
#include "CC_Serial.hh"
#include "Encoders.hh"

#ifdef ENABLE_GPS
#include <Adafruit_GPS.h>
#endif

#ifdef PBL
#undef PBL
#endif
#define PBL 64 // print buffer length

#include "Claw.hh"

/* Define globally
 */
ShellCommand sc_hello("hello",    "hello",                         "Say hi :-)");
ShellCommand sc_prgps("gps",      "gps",                           "Get GPS reading and print.");
ShellCommand sc_rclaw("claw",     "claw",                          "Get RoboClaw status.");
ShellCommand sc_rmode("report",   "report [--list] [<mode>]",      "Get/Set/List reporting mode(s).");
ShellCommand sc_opmod("op-mode",  "op-mode [--list] [<mode>]",     "Get/Set/List operating mode(s).");
ShellCommand sc_shsum("summary",  "summary [on|off]",              "Print summary to shell.");
ShellCommand sc_motor("M",        "M [<speed> [<speed>]]",         "Set motor speed(s).");
ShellCommand sc_znpid("ZN",       "ZN [<Ku> [<Tu>]]",              "PID test, opt. with Ziegler-Nichols.");

const char *s_report_mode[] = {
  "  0 (none)\n",
  "  1 GPS-triggered reporting\n",
  "  2 Buggy 'actual' at 10ms intervals\n"
};
const unsigned char s_report_mode_count = sizeof(s_report_mode) / sizeof(const char *);

const char *s_op_mode[] = {
  "  0 Simple motor control (symmetric)\n",
  "  1 PID control (experimental)\n",
  "  2 Diagnostic\n"
};
const unsigned char s_op_mode_count = sizeof(s_op_mode) / sizeof(const char *);

class Central : public Timer, public CommaComms::CC_Responder, public ShellHandler {
private:
  ShellCommandList m_list;
  Shell  m_zero;
  Shell  m_five;
  Shell *m_last;

  CC_Serial s2;
  CC_Serial s4;

  char m_buffer[PBL]; // temporary print buffer

#ifdef ENABLE_GPS
  Adafruit_GPS *gps;
#endif

  elapsedMicros report;
  unsigned char reportMode;
  unsigned char opMode;
  bool bReportGenerated;
  bool bShellSummary;

public:
  Central() :
    m_list(this),
    m_zero(Serial,  m_list),
    m_five(Serial5, m_list),
    m_last(0),
    s2(Serial2, '2', this),
    s4(Serial4, '4', this),
#ifdef ENABLE_GPS
    gps(new Adafruit_GPS(&Serial3)),
#endif
    report(0),
    reportMode(0),
    opMode(0),
    bReportGenerated(false),
    bShellSummary(false)
  {
    m_list.add(sc_hello);
    m_list.add(sc_prgps);
    m_list.add(sc_rclaw);
    m_list.add(sc_rmode);
    m_list.add(sc_opmod);
    m_list.add(sc_shsum);
    m_list.add(sc_motor);
    m_list.add(sc_znpid);

#ifdef ENABLE_PID
    opMode = 1;
#endif
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
    if (opMode == 0) {
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
        s4.ui_print(buf);
        s4.ui();
      }
    }

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
      snprintf(m_buffer, PBL, "%6.2f %6.2f %6.2f %6.2f %6.2f", TB_Params.actual_FL, TB_Params.actual_BL, TB_Params.actual_FR, TB_Params.actual_BR, TB_Params.actual);
      s4.ui_print(m_buffer);
      s4.ui();
    }

    if (opMode == 1) {
      s_buggy_update(10); // see Claw.hh
    }
    if (opMode == 3) {
      if (!s_test_update(TB_Params.actual_FR, 10)) {
        s_roboclaw_set_M1(0); // right
        opMode = 2;
        if (m_last) {
          m_last->write("done!\n");
          s_test_plot(*m_last, m_buffer);
        }
      }
    }
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
        snprintf(m_buffer, PBL, "M: %d {%d %d} v: %.2f %.2f %.2f %.2f km/h", MSpeed, M1_actual, M2_actual, vs1, vs2, vs3, vs4);
        s2.command_print(m_buffer);

        if (bShellSummary && m_last) {
          m_last->writeIfAvailable(m_buffer);
          m_last->writeIfAvailable("\n");
        }
      }
    }
  }

  virtual void every_second() { // runs once every second
    // ...
  }

#ifdef ENABLE_GPS
  inline const char *gps_time() {
    snprintf(m_buffer, PBL, "%02d/%02d/20%02d,%02d.%02d,%02d.%04u,",
      (int) gps->day,
      (int) gps->month,
      (int) gps->year,
      (int) gps->hour,
      (int) gps->minute,
      (int) gps->seconds,
      (unsigned int) gps->milliseconds);
    return m_buffer;
  }
  inline const char *gps_latitude() {
    float coord = abs(gps->latitudeDegrees);
    int degrees = (int) coord;
    coord = (coord - (float) degrees) * 60;
    int minutes = (int) coord;
    coord = (coord - (float) minutes) * 60;

    snprintf(m_buffer, PBL, "%3d^%02d'%.4f\"%c,", degrees, minutes, coord, gps->lat ? gps->lat : ((gps->latitudeDegrees < 0) ? 'S' : 'N'));
    return m_buffer;
  }
  inline const char *gps_longitude() {
    float coord = abs(gps->longitudeDegrees);
    int degrees = (int) coord;
    coord = (coord - (float) degrees) * 60;
    int minutes = (int) coord;
    coord = (coord - (float) minutes) * 60;

    snprintf(m_buffer, PBL, "%3d^%02d'%.4f\"%c,", degrees, minutes, coord, gps->lon ? gps->lon : ((gps->longitudeDegrees < 0) ? 'W' : 'E'));
    return m_buffer;
  }
  inline const char *gps_lat_lon() {
    snprintf(m_buffer, PBL, "%.6f,%.6f,", gps->latitudeDegrees, gps->longitudeDegrees);
    return m_buffer;
  }
#endif // ENABLE_GPS
  inline const char *summary_report(bool& bMoving, bool& bActive, bool bCSV = false) {
    float vs1 = TB_Params.actual_FL; // Vehicle speed in km/h
    float vs2 = TB_Params.actual_BL;
    float vs3 = TB_Params.actual_FR;
    float vs4 = TB_Params.actual_BR;

    bMoving = vs1 || vs2 || vs3 || vs4;
    bActive = MSpeed || M1_actual || M2_actual;

    const char *csv_format = "%3d,%3d,%3d,%.3f,%.3f,%.3f,%.3f";
    const char *ext_format = "M: %d {%d %d} v: %.2f %.2f %.2f %.2f km/h";
    snprintf(m_buffer, PBL, bCSV ? csv_format : ext_format, MSpeed, M1_actual, M2_actual, vs1, vs2, vs3, vs4);
    return m_buffer;
  }
  inline const char *summary_report() {
    bool tmp1, tmp2;
    return summary_report(tmp1, tmp2, true);
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
    s4.ui_print(buf);

    if (gps->fix) {
      float coord = abs(gps->latitudeDegrees);
      int degrees = (int) coord;
      coord = (coord - (float) degrees) * 60;
      int minutes = (int) coord;
      coord = (coord - (float) minutes) * 60;
            
      snprintf(buf, 48, "%3d^%02d'%.4f\"%c,", degrees, minutes, coord, gps->lat ? gps->lat : ((gps->latitudeDegrees < 0) ? 'S' : 'N'));
      s4.ui_print(buf);
            
      coord = abs(gps->longitudeDegrees);
      degrees = (int) coord;
      coord = (coord - (float) degrees) * 60;
      minutes = (int) coord;
      coord = (coord - (float) minutes) * 60;
            
      snprintf(buf, 48, "%3d^%02d'%.4f\"%c,", degrees, minutes, coord, gps->lon ? gps->lon : ((gps->longitudeDegrees < 0) ? 'W' : 'E'));
      s4.ui_print(buf);

      snprintf(buf, 48, "%.6f,%.6f,", gps->latitudeDegrees, gps->longitudeDegrees);
      s4.ui_print(buf);
    } else {
      s4.ui_print(",,,,");
    }
#endif

    snprintf(buf, 48, "%10lu,", (unsigned long) millis());
    s4.ui_print(buf);

    snprintf(buf, 48, "%3d,%3d,%3d,", MSpeed, M1_actual, M2_actual);
    s4.ui_print(buf);

    float vs1 = TB_Params.actual_FL; // Vehicle speed in km/h
    float vs2 = TB_Params.actual_BL;
    float vs3 = TB_Params.actual_FR;
    float vs4 = TB_Params.actual_BR;

    snprintf(buf, 48, "%.3f,%.3f,%.3f,%.3f", vs1, vs2, vs3, vs4);
    s4.ui_print(buf);
    s4.ui();

    bReportGenerated = true;
  }

  virtual void tick() {
    s2.update(); // important: housekeeping
    s4.update(); // important: housekeeping

    m_zero.tick();
    m_five.tick();

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
  void cmd_gps(Shell& origin) {
    origin.write("GPS: ");
#ifndef ENABLE_GPS
    origin.write("(disabled)\n");
#else
    bool bTimeOut = true;
    bool bNMEA = false;

    unsigned long t0 = millis();

    while (millis() - t0 < 1000) {
      if (gps->available()) {
        gps->read();
        bTimeOut = false;
        if (gps->newNMEAreceived()) {
          bNMEA = true;
          break;
        }
      }
    }
    if (bTimeOut) {
      origin.write("Timeout\n");
      return;
    }
    if (!bNMEA) {
      origin.write("No NMEA\n");
      return;
    }
    origin.write("NMEA: ");

    if (!gps->parse(gps->lastNMEA())) {
      origin.write("Parse error\n");
      return;
    }
    origin.write("OK\n");

    origin.write(gps_time());
    if (!gps->fix) {
      origin.write(" (no fix)\n");
      return;
    }
    origin.write(gps_latitude());
    origin.write(gps_longitude());
    origin.write(gps_lat_lon());
    origin.eol();
#endif
  }
  void cmd_claw(Shell& origin) {
    origin.write("RoboClaw: ");
#ifndef ENABLE_ROBOCLAW
    origin.write("(disabled)");
#else
    if (s_claw_writable) {
      origin.write("OK");
    } else {
      origin.write(s_claw_error);
    }
#endif
    origin.eol();
  }
  virtual CommandError shell_command(Shell& origin, CommandArgs& args) {
    CommandError ce = ce_Okay;

    m_last = &origin;

    if (args == "hello") {
      origin.write("Hi!\n");
    }
    else if (args == "gps") {
      cmd_gps(origin);
    }
    else if (args == "claw") {
      cmd_claw(origin);
    }
    else if (args == "report") {
      ++args;
      if (args == "") { // print current reporting mode
        origin.write(s_report_mode[reportMode]);
      }
      else if (args == "--list") { // list reporting modes
        for (int ir = 0; ir < s_report_mode_count; ir++) {
          origin.write(s_report_mode[ir]);
        }
      }
      else { // set current reporting mode
        int ir = 0;
        if (sscanf(args.c_str(), "%d", &ir) == 1) {
          if ((ir >= 0) && (ir < s_report_mode_count)) {
            reportMode = ir;
            origin.write(s_report_mode[reportMode]);
          } else {
            origin.write("Invalid report mode\n");
            ce = ce_IncorrectUsage;
          }
        } else {
          origin.write("Expected integer\n");
          ce = ce_IncorrectUsage;
        }
      }
    }
    else if (args == "op-mode") {
      ++args;
      if (args == "") { // print current operating mode
        origin.write(s_op_mode[opMode]);
      }
      else if (args == "--list") { // list operating modes
        for (int ir = 0; ir < s_op_mode_count; ir++) {
          origin.write(s_op_mode[ir]);
        }
      }
      else { // set current operating mode
        int ir = 0;
        if (sscanf(args.c_str(), "%d", &ir) == 1) {
          if ((ir >= 0) && (ir < s_op_mode_count)) {
            opMode = ir;
            origin.write(s_op_mode[opMode]);
          } else {
            origin.write("Invalid operating mode\n");
            ce = ce_IncorrectUsage;
          }
        } else {
          origin.write("Expected integer\n");
          ce = ce_IncorrectUsage;
        }
      }
    }
    else if (args == "summary") {
      ++args;
      if (args == "") {
        origin.write("Shell summary: ");
        origin.write(bShellSummary ? "on" : "off");
        origin.eol();
      }
      else if (args == "on") {
        bShellSummary = true;
      }
      else if (args == "off") {
        bShellSummary = false;
      } else {
        ce = ce_IncorrectUsage;
      }
    }
    else if (args == "M") {
      ++args;
      if (args == "") { // emergency stop
        MSpeed = 0;
        TB_Params.target = 0.0;
      } else if (opMode == 0) {
        int ir = 0;
        if (sscanf(args.c_str(), "%d", &ir) == 1) {
          if ((ir >= -127) && (ir <= 127)) {
            MSpeed = ir;
            TB_Params.target = (float) MSpeed / 10.0;
          } else {
            origin.write("Invalid speed\n");
            ce = ce_IncorrectUsage;
          }
        } else {
          origin.write("Expected integer\n");
          ce = ce_IncorrectUsage;
        }
      } else if (opMode == 2) {
        int ir1 = 0;
        int ir2 = 0;
        if (sscanf(args.c_str(), "%d", &ir1) == 1) {
          if ((ir1 >= -127) && (ir1 <= 127)) {
            ++args;
            if (sscanf(args.c_str(), "%d", &ir2) == 1) {
              if ((ir2 >= -127) && (ir2 <= 127)) {
                MSpeed = 0;
                TB_Params.target = (float) MSpeed / 10.0;
                s_roboclaw_set_M2(ir1); // left
                s_roboclaw_set_M1(ir2); // right
              } else {
                origin.write("Invalid speed\n");
                ce = ce_IncorrectUsage;
              }
            } else {
              origin.write("Expected two integers\n");
              ce = ce_IncorrectUsage;
            }
          } else {
            origin.write("Invalid speed\n");
            ce = ce_IncorrectUsage;
          }
        } else {
          origin.write("Expected two integers\n");
          ce = ce_IncorrectUsage;
        }
      }
    }
    else if (args == "ZN") {
      float Ku = 1;
      float Tu_ms = 0;
      ++args;
      if (args != "") {
        if (sscanf(args.c_str(), "%f", &Ku) == 1) {
          if (Ku <= 0) {
            origin.write("Expected Ku > 0\n");
            ce = ce_IncorrectUsage;
          }
          ++args;
          if (args != "") {
            if (sscanf(args.c_str(), "%f", &Tu_ms) == 1) {
              if (Tu_ms <= 0) {
                origin.write("Expected Tu_ms > 0\n");
                ce = ce_IncorrectUsage;
              }
            } else {
              origin.write("Expected float\n");
              ce = ce_IncorrectUsage;
            }
          }
        } else {
          origin.write("Expected float\n");
          ce = ce_IncorrectUsage;
        }
      }
      if ((ce == ce_Okay) && (opMode == 2)) {
        origin.write("Testing... ");
        s_test_init(Ku, Tu_ms);
        opMode = 3;
      }
    }
    return ce;
  }
};

void setup() {
  while (millis() < 500) {
    if (Serial) break;
  }
  Serial.begin(115200);

  while (millis() < 100) {
    if (Serial2) break;
  }
  Serial2.begin(115200);

  while (millis() < 100) {
    if (Serial4) break;
  }
  Serial4.begin(115200);

  while (millis() < 100) {
    if (Serial5) break;
  }
  Serial5.begin(115200);

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
