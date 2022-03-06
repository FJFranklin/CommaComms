/* Copyright 2022 Francis James Franklin
 * 
 * Adapted from examples in the https://github.com/greiman/SdFat library.
 * 
 * Open Source under the MIT License - see LICENSE in the project's root folder
 */

#ifndef CommaComms_LoggerSD_hh
#define CommaComms_LoggerSD_hh

#include <SdFat.h>
#include <sdios.h>
#include <RingBuf.h>

#include "MultiShell.hh"

#define LOG_FILE_SIZE     150000000
#define RING_BUF_CAPACITY    204800

class LoggerSD {
private:
  bool m_bLogging;

  SdioConfig m_config;

  SdFs m_sd;

  cid_t m_cid;
  csd_t m_csd;

  uint32_t m_ocr;

  MbrSector_t m_mbr;

  FsFile m_logfile;
  RingBuf<FsFile, RING_BUF_CAPACITY> m_rbuf;

public:
  LoggerSD() :
    m_bLogging(false),
    m_config(FIFO_SDIO)
  {
    // ...
  }

  ~LoggerSD() {
    // ...
  }

  bool init(unsigned long *dt = 0, Shell *shell = 0) {
    uint32_t t0 = millis();

    if (!m_sd.cardBegin(m_config)) {
      if (shell) {
	shell->write("SD: initialization failed.\n");
	if (isSpi(m_config)) {
	  shell->write("SD is a SPI device - check CS pin, etc.\n");
	}
      }
      return false;
    }
    if (dt)
      *dt = millis() - t0;

    if (!m_sd.card()->readCID(&m_cid) || !m_sd.card()->readCSD(&m_csd) || !m_sd.card()->readOCR(&m_ocr)) {
      if (shell)
        shell->write("SD: readInfo failed\n");
      return false;
    }

    if (!m_sd.card()->readSector(0, (uint8_t*) &m_mbr)) {
      if (shell)
        shell->write("SD: MBR read failed.\n");
      return false;
    }

    return true;
  }
  inline bool init(Shell& shell) {
    return init(0, &shell);
  }

  bool cmd_sd_info(Shell& origin) {
    char buf[64];

    origin.triple("SdFat version: ", SD_FAT_VERSION_STR, "\n");

    unsigned long dt;
    if (!init(&dt, &origin))
      return false;

    sprintf(buf, "SD: init time: %ldms\n", dt);
    origin.write(buf);

    const char * card_type = "Unknown";

    switch (m_sd.card()->type()) {
    case SD_CARD_TYPE_SD1:
      card_type = "SD1";
      break;

    case SD_CARD_TYPE_SD2:
      card_type = "SD2";
      break;

    case SD_CARD_TYPE_SDHC:
      if (sdCardCapacity(&m_csd) < 70000000) {
        card_type = "SDHC";
      } else {
        card_type = "SDXC";
      }
      break;

    default:
      break;
    }
    origin.triple("SD: card type: ", card_type, "\n");

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

    bool valid = true;

    origin.write("SD: Partition Table\n");

    for (uint8_t ip = 1; ip < 5; ip++) {
      MbrPart_t *pt = &m_mbr.part[ip - 1];
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

  bool cmd_ls(Shell& origin, const char *path = "/") {
    if (!init(origin))
      return false;

    if (!m_sd.volumeBegin()) {
      origin.write("SD: Error! Filesystem error\n");
      return false;
    }

    if (!m_sd.exists(path)) {
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

    m_sd.ls(path, LS_DATE | LS_SIZE);

    return true;
  }

  inline bool logging() const { return m_bLogging; }

  bool cmd_log_start(const char *path, Shell *shell = 0) {
    if (m_bLogging) {
      if (shell)
        shell->write("Logging is already active.\n");
      return false;
    }

    if (!init(0, shell))
      return false;

    if (!m_sd.volumeBegin()) {
      if (shell)
        shell->write("SD: Error! Filesystem error\n");
      return false;
    }

    if (!m_logfile.open(path, O_RDWR | O_CREAT | O_TRUNC)) {
      if (shell)
        shell->triple("Error! Failed to open ", path, " for writing.\n");
      return false;
    }

    if (!m_logfile.preAllocate(LOG_FILE_SIZE)) {
      if (shell)
        shell->write("Error! preAllocate failed\n");
      m_logfile.close();
      return false;
    }

    m_rbuf.begin(&m_logfile);

    m_bLogging = true;
    return true;
  }

  void cmd_log_stop(Shell *shell = 0) {
    if (!m_bLogging) {
      if (shell)
        shell->writeIfAvailable("Logger is not active.\n");
      return;
    }

    m_rbuf.sync();
    m_logfile.truncate();
    m_logfile.close();

    m_bLogging = false;
  }

  void cmd_log_write(const char *data, int length, Shell *shell = 0) {
    if (!m_bLogging)
      return;

    size_t n = m_rbuf.bytesUsed();
    if ((n + m_logfile.curPosition()) > (LOG_FILE_SIZE - 20)) {
      if (shell)
        shell->writeIfAvailable("Logger: File full - quitting.\n");
      cmd_log_stop();
      return;
    }

    m_rbuf.write(data, length);

    if (m_rbuf.getWriteError()) {
      if (shell)
        shell->writeIfAvailable("Logger: write error (data too fast for logging)\n");
    }
  }

  void cmd_log_tick(Shell *shell = 0) {
    if (!m_bLogging)
      return;

    if (m_rbuf.bytesUsed() >= 512 && !m_logfile.isBusy()) {
      if (512 != m_rbuf.writeOut(512)) {
        if (shell)
          shell->writeIfAvailable("Logger: write error - quitting.");
        cmd_log_stop();
      }
    }
  }
};

#endif /* !CommaComms_LoggerSD_hh */
