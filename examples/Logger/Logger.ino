#include <EEPROM.h>

#include <SdFat.h>
#include <sdios.h>

#include "MultiShell.hh"

ShellCommand sc_hello("hello",    "hello",                         "Say hi :-)");
ShellCommand sc_lnext("log-next", "log-next [--reset]",            "Next filename to be used when logging.");
ShellCommand sc_lauto("log-auto", "log-auto [--enable|--disable]", "Whether to start logging automatically on power-up.");
ShellCommand sc_sdinf("sd-info",  "sd-info",                       "Give details of SD card.");
ShellCommand sc_lsdir("ls",       "ls",                            "List files on the device.");

#include "Timer.hh"

class App : public Timer, public ShellHandler {
private:
  ShellCommandList m_list;
  Shell m_one;
  Shell m_two;

  bool bLogging;

  char log_filename_current[16];
  char log_filename_next[16];

public:
  App() :
    m_list(this),
    m_one(Serial, m_list),
    m_two(Serial2, m_list),
    bLogging(false)
  {
    pinMode(LED_BUILTIN, OUTPUT);

    m_list.add(sc_hello);
    m_list.add(sc_lnext);
    m_list.add(sc_lauto);
    m_list.add(sc_sdinf);
    m_list.add(sc_lsdir);
    // ...
  }
  ~App() {
    // ...
  }

  virtual void every_milli() { // runs once a millisecond, on average
    // ...
  }

  virtual void every_10ms() { // runs once every 10ms, on average
    //Serial3.println(analogRead(A0));
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

    /*while (Serial && Serial1.available()) {
      Serial.write(Serial1.read());
    }*/
  }

  bool cmd_sd_info(Shell& origin) {
    char buf[64];

    origin.write("SdFat version: ");
    origin.write(SD_FAT_VERSION_STR);
    origin.eol();

    SdioConfig config(FIFO_SDIO);

    uint32_t t = millis();
    SdFs sd;
    if (!sd.cardBegin(config)) {
      origin.write("SD: initialization failed.\n");
      if (isSpi(config)) {
        origin.write("SD is a SPI device - check CS pin, etc.\n");
      }
      return false;
    }
    t = millis() - t;
    sprintf(buf, "SD: init time: %ldms\n", t);
    origin.write(buf);

    cid_t m_cid;
    csd_t m_csd;
    uint32_t m_ocr;

    if (!sd.card()->readCID(&m_cid) || !sd.card()->readCSD(&m_csd) || !sd.card()->readOCR(&m_ocr)) {
      origin.write("SD: readInfo failed\n");
      return false;
    }

    origin.write("SD: card type: ");

    switch (sd.card()->type()) {
    case SD_CARD_TYPE_SD1:
      origin.write("SD1\n");
      break;

    case SD_CARD_TYPE_SD2:
      origin.write("SD2\n");
      break;

    case SD_CARD_TYPE_SDHC:
      if (sdCardCapacity(&m_csd) < 70000000) {
        origin.write("SDHC\n");
      } else {
        origin.write("SDXC\n");
      }
      break;

    default:
      origin.write("Unknown\n");
    }

    sprintf(buf, "SD: Manufacturer ID: %X; OEM ID: %c%c; Product: ", int(m_cid.mid), m_cid.oid[0], m_cid.oid[1]);
    origin.write(buf);
    for (uint8_t i = 0; i < 5; i++) {
      buf[i] = m_cid.pnm[i];
    }
    buf[5] = 0;
    origin.write(buf);
    sprintf(buf, "; Version: %d.%d\n", int(m_cid.prv_n), int(m_cid.prv_m));
    origin.write(buf);
    sprintf(buf, "    Serial number: %lX", m_cid.psn);
    origin.write(buf);
    sprintf(buf, "; Manufacturing date: %d/%d", int(m_cid.mdt_month), (2000 + 16*m_cid.mdt_year_high + m_cid.mdt_year_low));
    origin.write(buf);
    sprintf(buf, "; OCR: %lX\n", m_ocr);
    origin.write(buf);

    MbrSector_t mbr;

    if (!sd.card()->readSector(0, (uint8_t*) &mbr)) {
      origin.write("SD: MBR read failed.\n");
    }

    bool valid = true;

    origin.write("SD: Partition Table\n");

    for (uint8_t ip = 1; ip < 5; ip++) {
      MbrPart_t *pt = &mbr.part[ip - 1];
      if ((pt->boot != 0 && pt->boot != 0x80) || getLe32(pt->relativeSectors) > sdCardCapacity(&m_csd)) {
        valid = false;
      }
      char boot = (pt->boot == 0) ? '-' : ((pt->boot == 0x80) ? '*' : '?');
      sprintf(buf, "  %c %d %02x %03d:%03d:%03d %03d:%03d:%03d %10ld %10ld\n", boot, ip, int(pt->type),
              int(pt->beginCHS[0]), int(pt->beginCHS[1]), int(pt->beginCHS[2]), int(pt->endCHS[0]), int(pt->endCHS[1]), int(pt->endCHS[2]),
              getLe32(pt->relativeSectors), getLe32(pt->totalSectors));
      origin.write(buf);
    }
    if (!valid) {
      origin.write("SD: MBR not valid, assuming Super Floppy format.\n");
    }

    return true;    
  }

  bool cmd_ls(Shell& origin, const char * path = "/") {
    SdFs sd;
    if (!sd.cardBegin(SdioConfig(FIFO_SDIO))) {
      origin.write("SD: Error! No card?\n");
      return false;
    }
    if (!sd.volumeBegin()) {
      origin.write("SD: Error! Filesystem error\n");
      return false;
    }

    if (!sd.exists(path)) {
      origin.triple("SD: Error! '", path, "' not found\n");
      return false;
    }

    FsFile file;
    if (!file.open(path, O_RDONLY)) {
      origin.triple("SD: Error! Unable to read '", path, "'\n");
      return false;
    }

    if (file.isDir()) {
      origin.write("SD: Folder\n");
    } else {
      origin.write("SD: File\n");
    }

    sd.ls(path, LS_DATE | LS_SIZE);

    return true;
  }

  const char * log_name_next() {
    unsigned int lo = EEPROM.read(0);
    unsigned int hi = EEPROM.read(1);

    sprintf(log_filename_next, "log-%u.txt", lo | (hi << 8));

    return log_filename_next;
  }

  bool auto_start() {
    return EEPROM.read(2);
  }

  virtual CommandError shell_command(Shell& origin, CommandArgs& args) {
    CommandError ce = ce_Okay;

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
      cmd_sd_info(origin);
    } else if (args == "ls") {
      cmd_ls(origin);
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
