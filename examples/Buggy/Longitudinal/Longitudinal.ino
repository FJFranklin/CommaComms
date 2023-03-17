/* -*- mode: c++ -*-
 * 
 * Copyright 2020-23 Francis James Franklin
 * 
 * Open Source under the MIT License - see LICENSE in the project's root folder
 */

#include <Shell.hh>

using namespace MultiShell;

#ifdef Central_BufferLength
#undef Central_BufferLength
#endif
#define Central_BufferLength 128 // print buffer length

#include "config.hh"
#include "Encoders.hh"
#include "Claw.hh"

/* Define globally
 */
ShellStream serial_zero(Serial);
ShellStream serial_five(Serial5);

Command sc_hello("hello",    "hello",                         "Say hi :-)");
Command sc_rclaw("claw",     "claw",                          "Get RoboClaw status.");
Command sc_rmode("report",   "report [--list] [<mode>]",      "Get/Set/List reporting mode(s).");
Command sc_opmod("op-mode",  "op-mode [--list] [<mode>]",     "Get/Set/List operating mode(s).");
Command sc_shsum("summary",  "summary [on|off]",              "Print summary to shell.");
Command sc_motor("M",        "M [<speed> [<speed>]]",         "Set motor speed(s).");
Command sc_znpid("ZN",       "ZN [<Ku> [<Tu>]]",              "PID test, opt. with Ziegler-Nichols.");

const char *s_report_mode[] = {
  "  0 (none)",
  "  1 GPS-triggered reporting",
  "  2 Buggy 'actual' at 10ms intervals"
};
const unsigned char s_report_mode_count = sizeof(s_report_mode) / sizeof(const char *);

const char *s_op_mode[] = {
  "  0 Simple motor control (symmetric)",
  "  1 PID control (experimental)",
  "  2 Diagnostic"
};
const unsigned char s_op_mode_count = sizeof(s_op_mode) / sizeof(const char *);

class Longitudinal : public Timer, public ShellHandler {
private:
  CommandList m_list;
  Shell  m_zero;
  Shell  m_five;
  Shell *m_last;

  char m_buffer[Central_BufferLength]; // temporary print buffer
  ShellBuffer m_B;

  unsigned char reportMode;
  unsigned char opMode;
  bool bShellSummary;

public:
  Longitudinal() :
    m_list(this),
    m_zero(serial_zero, m_list, '0'),
    m_five(serial_five, m_list, '5'),
    m_last(0),
    m_B(m_buffer, Central_BufferLength),
    reportMode(0),
    opMode(0),
    bShellSummary(false)
  {
    m_list.add(sc_hello);
    m_list.add(sc_rclaw);
    m_list.add(sc_rmode);
    m_list.add(sc_opmod);
    m_list.add(sc_shsum);
    m_list.add(sc_motor);
    m_list.add(sc_znpid);

#ifdef ENABLE_PID
    opMode = 1;
#endif
  }
  virtual ~Longitudinal() {
    // ...
  }

  virtual void shell_notification(Shell& origin, const char *message) {
    if (Serial) {
      Serial.print(origin.name());
      Serial.print(": notify: ");
      Serial.println(message);
    }
  }
  virtual void comma_command(Shell& origin, CommaCommand& command) {
#ifdef ENABLE_FEEDBACK
    if (Serial) {
      Serial.print(origin.name());
      Serial.print(": command: ");
      Serial.print(command.m_command);
      Serial.print(": ");
      Serial.println(command.m_value);
    }
#endif
    unsigned long value = command.m_value;

    switch(command.m_command) {
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

    if (opMode == 1) {
      s_buggy_update(10); // see Claw.hh
    }
    if (opMode == 3) {
      if (!s_test_update(TB_Params.actual_FR, 10)) {
        s_roboclaw_set_M1(0); // right
        opMode = 2;
        if (m_last) {
          *m_last << "done!" << 0;
          s_test_plot(*m_last, m_B);
        }
      }
    }
  }

  virtual void every_tenth(int tenth) { // runs once every tenth of a second, where tenth = 0..9
    digitalWrite(LED_BUILTIN, tenth == 0 || tenth == 8);

    if (tenth == 0 || tenth == 5) { // i.e., every half-second
      bool bMoving = false;
      bool bActive = false;

      m_B.clear();
      summary_report(m_B, bMoving, bActive);

      if (bMoving || bActive) {
        if (bShellSummary && m_last) {
          *m_last << m_B << 0;
        }
      }
    }
  }

  virtual void every_second() { // runs once every second
    // ...
  }

  void summary_report(ShellBuffer& B, bool& bMoving, bool& bActive, bool bCSV = false) {
    float vs1 = TB_Params.actual_FL; // Vehicle speed in km/h
    float vs2 = TB_Params.actual_BL;
    float vs3 = TB_Params.actual_FR;
    float vs4 = TB_Params.actual_BR;

    bMoving = vs1 || vs2 || vs3 || vs4;
    bActive = MSpeed || M1_actual || M2_actual;

    const char *csv_format = "%3d,%3d,%3d,%.3f,%.3f,%.3f,%.3f";
    const char *ext_format = "M: %d {%d %d} v: %.2f %.2f %.2f %.2f km/h";
    B.printf(bCSV ? csv_format : ext_format, MSpeed, M1_actual, M2_actual, vs1, vs2, vs3, vs4);
  }
  inline void summary_report(ShellBuffer& B) {
    bool tmp1, tmp2;
    summary_report(B, tmp1, tmp2, true);
  }

  virtual void tick() {
    m_zero.update();
    m_five.update();
  }
  void cmd_claw(Shell& origin) {
    origin << "RoboClaw: ";
#ifndef ENABLE_ROBOCLAW
    origin << "(disabled)" << 0;
#else
    if (s_claw_writable) {
      origin << "OK" << 0;
    } else {
      origin << s_claw_error << 0;
    }
#endif
  }
  virtual CommandError shell_command(Shell& origin, Args& args) {
    CommandError ce = ce_Okay;

    m_last = &origin;

    if (args == "hello") {
      origin << "Hi!" << 0;
    }
    else if (args == "claw") {
      cmd_claw(origin);
    }
    else if (args == "report") {
      ++args;
      if (args == "") { // print current reporting mode
        origin << s_report_mode[reportMode] << 0;
      }
      else if (args == "--list") { // list reporting modes
        for (int ir = 0; ir < s_report_mode_count; ir++) {
          origin << s_report_mode[ir] << 0;
        }
      }
      else { // set current reporting mode
        int ir = 0;
        if (sscanf(args.c_str(), "%d", &ir) == 1) {
          if ((ir >= 0) && (ir < s_report_mode_count)) {
            reportMode = ir;
            origin << s_report_mode[reportMode] << 0;
          } else {
            origin << "Invalid report mode" << 0;
            ce = ce_IncorrectUsage;
          }
        } else {
          origin << "Expected integer" << 0;
          ce = ce_IncorrectUsage;
        }
      }
    }
    else if (args == "op-mode") {
      ++args;
      if (args == "") { // print current operating mode
        origin << s_op_mode[opMode] << 0;
      }
      else if (args == "--list") { // list operating modes
        for (int ir = 0; ir < s_op_mode_count; ir++) {
          origin << s_op_mode[ir] << 0;
        }
      }
      else { // set current operating mode
        int ir = 0;
        if (sscanf(args.c_str(), "%d", &ir) == 1) {
          if ((ir >= 0) && (ir < s_op_mode_count)) {
            opMode = ir;
            origin << s_op_mode[opMode] << 0;
          } else {
            origin << "Invalid operating mode" << 0;
            ce = ce_IncorrectUsage;
          }
        } else {
          origin << "Expected integer" << 0;
          ce = ce_IncorrectUsage;
        }
      }
    }
    else if (args == "summary") {
      ++args;
      if (args == "") {
        origin << "Shell summary: " << (bShellSummary ? "on" : "off") << "" << 0;
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
            origin << "Invalid speed" << 0;
            ce = ce_IncorrectUsage;
          }
        } else {
          origin << "Expected integer" << 0;
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
                origin << "Invalid speed" << 0;
                ce = ce_IncorrectUsage;
              }
            } else {
              origin << "Expected two integers" << 0;
              ce = ce_IncorrectUsage;
            }
          } else {
            origin << "Invalid speed" << 0;
            ce = ce_IncorrectUsage;
          }
        } else {
          origin << "Expected two integers" << 0;
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
            origin << "Expected Ku > 0" << 0;
            ce = ce_IncorrectUsage;
          }
          ++args;
          if (args != "") {
            if (sscanf(args.c_str(), "%f", &Tu_ms) == 1) {
              if (Tu_ms <= 0) {
                origin << "Expected Tu_ms > 0" << 0;
                ce = ce_IncorrectUsage;
              }
            } else {
              origin << "Expected float" << 0;
              ce = ce_IncorrectUsage;
            }
          }
        } else {
          origin << "Expected float" << 0;
          ce = ce_IncorrectUsage;
        }
      }
      if ((ce == ce_Okay) && (opMode == 2)) {
        origin << "Testing... ";
        s_test_init(Ku, Tu_ms);
        opMode = 3;
      }
    }
    return ce;
  }
};

void setup() {
  delay(500);

  serial_zero.begin(115200); // Shell on USB
  serial_five.begin(115200); // Shell (alternate)

  s_roboclaw_init(); // Setup RoboClaw

  E1.init(E1_ChA, E1_ChB, ENCODER_PPR, false); // set up encoder 1
  E2.init(E2_ChA, E2_ChB, ENCODER_PPR, false); // set up encoder 2
  E3.init(E3_ChA, E3_ChB, ENCODER_PPR, false); // set up encoder 3
  E4.init(E4_ChA, E4_ChB, ENCODER_PPR, false); // set up encoder 4

  pinMode(LED_BUILTIN, OUTPUT);

  Longitudinal().run();
}

void loop() {
  // ...
}
