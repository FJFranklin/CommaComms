#include <EEPROM.h>

#include "MultiShell.hh"
#include "Timer.hh"
#include "LoggerSD.hh"

/* Define globally
 */
ShellCommand sc_hello("hello",    "hello",                         "Say hi :-)");
ShellCommand sc_lnext("log-next", "log-next [--reset]",            "Next filename to be used when logging.");
ShellCommand sc_lauto("log-auto", "log-auto [--enable|--disable]", "Whether to start logging automatically on power-up.");
ShellCommand sc_sdinf("sd-info",  "sd-info",                       "Give details of SD card.");
ShellCommand sc_lsdir("ls",       "ls",                            "List files on the device.");
ShellCommand sc_mkdir("mkdir",    "mkdir <path>",                  "Create folder at path.");
ShellCommand sc_rmfle("rm",       "rm <path>",                     "Delete specified file.");
ShellCommand sc_catfl("cat",      "cat <path>",                    "Write contents of specified file.");
ShellCommand sc_demoj("demo",     "demo [--enable|--disable]",     "Generate junk data stream on Serial3.");
ShellCommand sc_logst("log",      "log start|stop|status",         "Start/Stop the logger, or check status.");
ShellCommand sc_trunc("truncate", "truncate <path>",               "Truncate preallocated log file after restart.");

class App : public Timer, public ShellHandler {
private:
  ShellCommandList m_list;
  Shell  m_one;
  Shell  m_two;
  Shell *m_last;

  bool bDemo;

  LoggerSD m_SD;

  char log_filename_current[16];
  char log_filename_next[16];

public:
  App() :
    m_list(this),
    m_one(Serial, m_list),
    m_two(Serial2, m_list),
    m_last(0),
    bDemo(false)
  {
    pinMode(LED_BUILTIN, OUTPUT);

    m_list.add(sc_hello);
    m_list.add(sc_lnext);
    m_list.add(sc_lauto);
    m_list.add(sc_sdinf);
    m_list.add(sc_lsdir);
    m_list.add(sc_mkdir);
    m_list.add(sc_rmfle);
    m_list.add(sc_catfl);
    m_list.add(sc_demoj);
    m_list.add(sc_logst);
    m_list.add(sc_trunc);

    if (auto_start()) {
      const char * log_name = log_name_next();
      strcpy(log_filename_current, log_name);
      log_name_increment();
      m_SD.cmd_log_start(log_name, m_last);
    }
  }
  ~App() {
    // ...
  }

  virtual void every_milli() { // runs once a millisecond, on average
    // ...
  }

  virtual void every_10ms() { // runs once every 10ms, on average
    if (bDemo) {
      Serial3.println(analogRead(A0));
    }
  }

  virtual void every_tenth(int tenth) { // runs once every tenth of a second, where tenth = 0..9
    digitalWrite(LED_BUILTIN, tenth == 0 || tenth == 8); // double blink per second
  }

  virtual void every_second() { // runs once every second
    // ...
  }

  virtual void tick() {
    m_one.tick();
    m_two.tick();

    m_SD.cmd_log_tick(m_last);

    if (m_SD.logging()) {
      char buffer[16];
      int count = 0;
      while (Serial1.available() && (count < 16)) {
        buffer[count++] = Serial1.read();
      }
      if (count) {
        m_SD.cmd_log_write(buffer, count, m_last);
      }
    }
  }

  const char * log_name_next() {
    unsigned int lo = EEPROM.read(0);
    unsigned int hi = EEPROM.read(1);

    sprintf(log_filename_next, "log-%u.txt", lo | (hi << 8));

    return log_filename_next;
  }

  void log_name_increment() {
    unsigned int lo = EEPROM.read(0);
    unsigned int hi = EEPROM.read(1);

    if (++lo == 256) {
      lo = 0;
      if (++hi == 256) {
        hi = 0;
      }
    }

    EEPROM.write(0, lo);
    EEPROM.write(1, hi);
  }

  bool auto_start() {
    return EEPROM.read(2);
  }

  virtual CommandError shell_command(Shell& origin, CommandArgs& args) {
    CommandError ce = ce_Okay;

    m_last = &origin;

    if (args == "hello") {
      origin.write("Hi!\n");
    } else if (args == "log-next") {
      if (++args == "--reset") {
        EEPROM.write(0, 0);
        EEPROM.write(1, 0);
      }
      origin.write(log_name_next());
      origin.eol();
    } else if (args == "log-auto") {
      if (++args != "") {
        if (args == "--enable")
          EEPROM.write(2, 1);
        else if (args == "--disable")
          EEPROM.write(2, 0);
      }
      if (auto_start()) {
        origin.write("autostart enabled\n");
      } else {
        origin.write("autostart disabled\n");
      }
    } else if (args == "sd-info") {
      m_SD.cmd_sd_info(origin);
    } else if (args == "ls") {
      if (++args == "")
        m_SD.cmd_ls(origin);
      else
        m_SD.cmd_ls(origin, args.c_str());
    } else if (args == "mkdir") {
      if (++args == "")
        ce = ce_IncorrectUsage;
      else
        m_SD.cmd_mkdir(args.c_str(), &origin);
    } else if (args == "rm") {
      if (++args == "")
        ce = ce_IncorrectUsage;
      else
        m_SD.cmd_rm(origin, args.c_str());
    } else if (args == "cat") {
      if (++args == "")
        ce = ce_IncorrectUsage;
      else
        m_SD.cmd_cat(origin, args.c_str());
    } else if (args == "demo") {
      if (++args != "") {
        if (args == "--enable")
          bDemo = true;
        else if (args == "--disable")
          bDemo = false;
      }
      origin.triple("Junk datastream ", bDemo ? "en" : "dis", "abled on Serial3.\n");
    } else if (args == "log") {
      if (++args != "") {
        if (args == "start") {
          if (m_SD.logging()) {
            origin.write("Logger is already active.\n");
          } else {
            origin.write("Logger: starting...\n");
            const char * log_name = log_name_next();
            strcpy(log_filename_current, log_name);
            log_name_increment();
            m_SD.cmd_log_start(log_name, &origin);
          }
        } else if (args == "stop") {
          if (!m_SD.logging()) {
            origin.write("Logger is not active.\n");
          } else {
            origin.write("Logger: stopping...\n");
            m_SD.cmd_log_stop(&origin);
          }
        } else if (args == "status") {
          origin.triple("Logger: ", m_SD.logging() ? "Active" : "Inactive", "\n");
        } else
          ce = ce_IncorrectUsage;
      } else {
        ce = ce_IncorrectUsage;
      }
    } else if (args == "truncate") {
      if (++args == "")
        ce = ce_IncorrectUsage;
      else
        m_SD.cmd_truncate(origin, args.c_str());
    } else {
      origin.write("Oops! Command: \"");
      origin.write(args.c_str());
      origin.write("\"\n");
      while (++args != "") {
        origin.write("          arg: \"");
        origin.write(args.c_str());
        origin.write("\"\n");
      }
      ce = ce_UnhandledCommand;
    }
    return ce;
  }
};

void setup() {
  // while (!Serial);
  Serial.begin(115200);
  Serial2.begin(115200);
  Serial1.begin(115200);
  Serial3.begin(115200);

  delay(1000);
  // Serial.println(Serial.read());

  App().run();
}

void loop() {
  // Not reached //
}
