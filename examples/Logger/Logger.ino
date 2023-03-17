/* -*- mode: c++ -*-
 * 
 * Copyright 2022 Francis James Franklin
 * 
 * Open Source under the MIT License - see LICENSE in the project's root folder
 */

#include <EEPROM.h>

#include <Adafruit_GPS.h>

#include <Shell.hh>

using namespace MultiShell;

#include "LoggerSD.hh"

class GPS : public Task {
private:
  Adafruit_GPS   m_gps;
  ShellBuffer   *m_buf;
  const char    *m_ptr;
  unsigned long  m_t0;
public:
  GPS(HardwareSerial& serial) :
    m_gps(&serial),
    m_buf(0),
    m_ptr(0),
    m_t0(0)
  {
    m_gps.begin(9600);
    m_gps.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA);
    m_gps.sendCommand(PMTK_API_SET_FIX_CTL_5HZ);
    m_gps.sendCommand(PMTK_SET_NMEA_UPDATE_5HZ);
  //m_gps.sendCommand(PMTK_SET_NMEA_UPDATE_1HZ);
  //m_gps.sendCommand(PGCMD_ANTENNA);
  //delay(1000);
  }
  virtual ~GPS() {
    // ...
  }
  inline void gps_time(ShellBuffer& B) {
    B.printf("%02d/%02d/20%02d,%02d.%02d,%02d.%04u,",
      (int) m_gps.day,
      (int) m_gps.month,
      (int) m_gps.year,
      (int) m_gps.hour,
      (int) m_gps.minute,
      (int) m_gps.seconds,
      (unsigned int) m_gps.milliseconds);
  }
  inline void gps_latitude(ShellBuffer& B) {
    float coord = abs(m_gps.latitudeDegrees);
    int degrees = (int) coord;
    coord = (coord - (float) degrees) * 60;
    int minutes = (int) coord;
    coord = (coord - (float) minutes) * 60;

    B.printf("%3d^%02d'%.4f\"%c,", degrees, minutes, coord, m_gps.lat ? m_gps.lat : ((m_gps.latitudeDegrees < 0) ? 'S' : 'N'));
  }
  inline void gps_longitude(ShellBuffer& B) {
    float coord = abs(m_gps.longitudeDegrees);
    int degrees = (int) coord;
    coord = (coord - (float) degrees) * 60;
    int minutes = (int) coord;
    coord = (coord - (float) minutes) * 60;

    B.printf("%3d^%02d'%.4f\"%c,", degrees, minutes, coord, m_gps.lon ? m_gps.lon : ((m_gps.longitudeDegrees < 0) ? 'W' : 'E'));
  }
  inline void gps_lat_lon(ShellBuffer& B) {
    B.printf("%.6f,%.6f,", m_gps.latitudeDegrees, m_gps.longitudeDegrees);
  }
  void summary(ShellBuffer& B) {
    gps_time(B);
    gps_latitude(B);
    gps_longitude(B);
    gps_lat_lon(B);
  }

  void query_gps(Shell& origin) {
    origin << "GPS: ";

    bool bTimeOut = true;
    bool bNMEA = false;

    unsigned long t0 = millis();

    while (millis() - t0 < 1000) {
      if (m_gps.available()) {
        m_gps.read();
        bTimeOut = false;
        if (m_gps.newNMEAreceived()) {
          bNMEA = true;
          break;
        }
      }
    }
    if (bTimeOut) {
      origin << "Timeout" << 0;
      return;
    }
    if (!bNMEA) {
      origin << "No NMEA" << 0;
      return;
    }
    origin << "NMEA: ";

    if (!m_gps.parse(m_gps.lastNMEA())) {
      origin << "Parse error" << 0;
      return;
    }
    origin << "OK" << 0;

    ShellBuffer *B = Shell::tmp_buffer();
    if (!B) {
      origin << 0;
      return;
    }

    gps_time(*B);
    origin << *B;

    if (!m_gps.fix) {
      origin << " (no fix)" << 0;
    } else {
      B->clear();
      gps_latitude(*B);
      gps_longitude(*B);
      gps_lat_lon(*B);
      origin << *B << 0;
    }
    B->return_to_owner();
  }

  bool update() {
    if (m_buf) { // busy - in task mode
      return false;
    }
    bool bFixed = false;

    if (m_gps.available()) {
      m_gps.read();
      if (m_gps.newNMEAreceived()) {
        if (m_gps.parse(m_gps.lastNMEA())) {
          bFixed = m_gps.fix;
        }
      }
    }
    return bFixed;
  }

  void assign(ShellBuffer& B) {
    B = "GPS: ";
    m_buf = &B;
    m_ptr = 0;
    m_t0 = millis();
  }
  virtual bool process_task(ShellStream& stream, int& afw) { // returns true on completion of task
    if (!m_ptr) {
      if (millis() - m_t0 > 1000) {
        *m_buf << "(timeout)";
        m_ptr = m_buf->c_str();
      } else {
        while (m_gps.available()) {
          m_gps.read();
          if (m_gps.newNMEAreceived()) {
            if (m_gps.parse(m_gps.lastNMEA())) {
              if (m_gps.fix) {
                *m_buf << "OK: ";
                summary(*m_buf);
                m_ptr = m_buf->c_str();
                break;
              } else {
                *m_buf << '?';
              }
            } else {
              *m_buf << '.';
            }
          }
        }
        if (!m_ptr) return false;
      }
    }
    int afr = strlen(m_ptr);

    while (afr && afw) {
      --afr;
      stream.write(*m_ptr++, afw);
    }
    if (!afr) {
      m_buf->return_to_owner();
      m_buf = 0;
      m_ptr = 0;
    }
    return afr == 0;
  }
};

/* Define globally
 */
ShellStream serial_one(Serial);
ShellStream serial_two(Serial2);
ShellStream serdata_in(Serial1);

Command sc_hello("hello",    "hello",                         "Say hi :-)");
Command sc_prgps("gps",      "gps",                           "Get GPS reading and print.");
Command sc_lnext("log-next", "log-next [--reset]",            "Next filename to be used when logging.");
Command sc_lauto("log-auto", "log-auto [--enable|--disable]", "Whether to start logging automatically on power-up.");
Command sc_sdinf("sd-info",  "sd-info",                       "Give details of SD card.");
Command sc_lsdir("ls",       "ls",                            "List files on the device.");
Command sc_mkdir("mkdir",    "mkdir <path>",                  "Create folder at path.");
Command sc_rmfle("rm",       "rm <path>",                     "Delete specified file.");
Command sc_catfl("cat",      "cat <path>",                    "Write contents of specified file.");
Command sc_demoj("demo",     "demo [--enable|--disable]",     "Generate junk data stream on Serial3.");
Command sc_logst("log",      "log start|stop|status",         "Start/Stop the logger, or check status.");
Command sc_trunc("truncate", "truncate <path>",               "Truncate preallocated log file after restart.");

class App : public Timer, public ShellHandler {
private:
  CommandList  m_list;

  Shell  m_one;
  Shell  m_two;
  Shell *m_last;

  bool  m_bDemo;

  LoggerSD  m_SD;

  char  log_filename_current[16];
  char  log_filename_next[16];
  char  log_data_tmp[16];

  ShellBuffer  m_log_current;
  ShellBuffer  m_log_next;
  ShellBuffer  m_log_data;

  GPS            *m_gps;
  TaskOwner<GPS>  m_gps_owner;
public:
  App(GPS& gps) :
    m_list(this),
    m_one(serial_one, m_list, '0'),
    m_two(serial_two, m_list, '2'),
    m_last(0),
    m_bDemo(false),
    m_log_current(log_filename_current, 16),
    m_log_next(log_filename_next, 16),
    m_log_data(log_data_tmp, 16),
    m_gps(&gps)
  {
    pinMode(LED_BUILTIN, OUTPUT);

    m_list.add(sc_hello);
    m_list.add(sc_prgps);
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

    m_gps_owner.push(gps, true);

    if (auto_start()) {
      m_log_current = log_name_next();
      log_name_increment();
      m_SD.cmd_log_start(m_log_current.c_str(), m_last);
    }
  }
  ~App() {
    // ...
  }

  void cmd_gps(Shell& origin) {
    ShellBuffer *B = Shell::tmp_buffer();
    if (B) {
      GPS *tptr = m_gps_owner.pop();
      if (!tptr) {
        B->return_to_owner();
      } else {
        tptr->assign(*B);
        origin << *tptr << 0;
      }
    }
  }

  virtual void every_milli() { // runs once a millisecond, on average
    // ...
  }

  virtual void every_10ms() { // runs once every 10ms, on average
    if (m_bDemo) {
      Serial3.println(analogRead(A0));
    }
    if (false /*m_gps->update()*/) {
      ShellBuffer *B = Shell::tmp_buffer();
      if (B) {
        m_gps->summary(*B);
        m_one << *B << 0;
        B->return_to_owner();
      }
    } else {
      //
    }
  }

  virtual void every_tenth(int tenth) { // runs once every tenth of a second, where tenth = 0..9
    digitalWrite(LED_BUILTIN, tenth == 0 || tenth == 8); // double blink per second
  }

  virtual void every_second() { // runs once every second
    //cmd_gps(m_one);
  }

  virtual void tick() {
    m_one.update();
    m_two.update();

    m_SD.cmd_log_tick(m_last);

    if (m_SD.logging()) {
      serdata_in.read(m_log_data.clear());
      if (m_log_data.count()) {
        m_SD.cmd_log_write(m_log_data, m_last);
      }
    }
    int ic = Serial2.read();
    if (ic > 0) Serial.write((char) ic);
  }

  ShellBuffer& log_name_next() {
    unsigned int lo = EEPROM.read(0);
    unsigned int hi = EEPROM.read(1);

    m_log_next.clear().printf("log-%u.txt", lo | (hi << 8));

    return m_log_next;
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

  virtual void shell_notification(Shell& origin, const char *message) {
    // ...
  }

  virtual void comma_command(Shell& origin, CommaCommand& command) {
    // ...
  }

  virtual CommandError shell_command(Shell& origin, Args& args) {
    CommandError ce = ce_Okay;

    m_last = &origin;

    if (args == "hello") {
      origin << "Hi!" << 0;
    } else if (args == "gps") {
      cmd_gps(origin); // m_gps->query_gps(origin);
    } else if (args == "log-next") {
      if (++args == "--reset") {
        EEPROM.write(0, 0);
        EEPROM.write(1, 0);
      }
      origin << log_name_next() << 0;
    } else if (args == "log-auto") {
      if (++args != "") {
        if (args == "--enable")
          EEPROM.write(2, 1);
        else if (args == "--disable")
          EEPROM.write(2, 0);
      }
      if (auto_start()) {
        origin << "autostart enabled" << 0;
      } else {
        origin << "autostart disabled" << 0;
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
          m_bDemo = true;
        else if (args == "--disable")
          m_bDemo = false;
      }
      origin << "Junk datastream " << (m_bDemo ? "en" : "dis") << "abled on Serial3." << 0;
    } else if (args == "log") {
      if (++args != "") {
        if (args == "start") {
          if (m_SD.logging()) {
            origin << "Logger is already active." << 0;
          } else {
            origin << "Logger: starting..." << 0;
            m_log_current = log_name_next();
            log_name_increment();
            m_SD.cmd_log_start(m_log_current.c_str(), &origin);
          }
        } else if (args == "stop") {
          if (!m_SD.logging()) {
            origin << "Logger is not active." << 0;
          } else {
            origin << "Logger: stopping..." << 0;
            m_SD.cmd_log_stop(&origin);
          }
        } else if (args == "status") {
          origin << "Logger: " << (m_SD.logging() ? "Active" : "Inactive") << 0;
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
      origin << "Oops! Command: \"" << args << "\"" << 0;
      while (++args != "") {
        origin << "          arg: \"" << args << "\"" << 0;
      }
      ce = ce_UnhandledCommand;
    }
    return ce;
  }
};

void setup() {
  delay(500);

  while (!Serial4);
  GPS gps(Serial4);

  serial_one.begin(115200); // Shell @ USB
  serial_two.begin(115200); // Shell @ Serial2

  serdata_in.begin(115200); // Data-in for logger

  Serial3.begin(115200); // Data-out junk for testing

  delay(500);

  App(gps).run();
}

void loop() {
  // Not reached //
}
