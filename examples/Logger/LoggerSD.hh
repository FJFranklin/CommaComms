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

#define LOG_FILE_SIZE     150000000
#define RING_BUF_CAPACITY    204800

class SDSerial : public VirtualSerial {
private:
  FsFile  m_file;
  bool    m_bDone;
public:
  SDSerial() : m_bDone(true)
  {
    // ...
  }

  virtual ~SDSerial();

  virtual bool begin(const char *& status, unsigned long baud);

  bool open(Shell& origin, ShellBuffer& B, const char *path) {
    if (!m_file.open(path, O_RDONLY)) {
      B << "SD: Error! Unable to read '" << path << "'";
      origin << B << 0;
      return false;
    }
    if (m_file.isDir()) {
      origin << "SD: Error! This is a folder" << 0;
      m_file.close();
      return false;
    }
    m_bDone = false;
    return true;
  }
  inline int sync_read_begin() {
    int afr = m_in.available();
    if (!afr) {
      sync_read();
      afr = m_in.available();
    }
    return afr;
  }
  inline int read(int& afr) {
    int ic = VirtualSerial::read();
    if (ic > -1)
      --afr;
    return ic;
  }
  inline bool done() const { return m_bDone && m_in.is_empty(); }

  virtual void sync_read();
  virtual void sync_write();
};

SDSerial::~SDSerial() {
  // ...
}

bool SDSerial::begin(const char *& status, unsigned long baud) {
  return true;
}

void SDSerial::sync_read() {
  if (m_bDone) return;

  int afw = m_in.availableForWrite();
  int count = m_file.read(m_buffer_out, afw); // use the protected out-buffer for convenience

  if (count > 0) {
    m_in.write(m_buffer_out, count);
  }
  if (count < afw) {
    m_file.close();
    m_bDone = true;
  }
}

void SDSerial::sync_write() {
  // ...
}

class LoggerCatTask : public Task {
private:
  SDSerial    m_file;
public:
  LoggerCatTask() {
    // ...
  }
  virtual ~LoggerCatTask() {
    // ...
  }
  bool assign(Shell& origin, ShellBuffer& B, const char *path) {
    return m_file.open(origin, B, path);
  }
  virtual bool process_task(ShellStream& stream, int& afw) { // returns true on completion of task
    int afr = m_file.sync_read_begin();

    while (afr && afw) {
      int ic = m_file.read(afr);
      if (ic < 0)
        break;
      char c = ic;
      stream.write(c, afw);
    }
    return m_file.done();
  }
};

class LoggerSD {
private:
  LoggerCatTask             m_cat_task;
  TaskOwner<LoggerCatTask>  m_cat_owner;

  char        m_buftmp[128];
  ShellBuffer m_B;

  bool m_bLogging;
  bool m_bHaveMeta;

  SdioConfig m_config;

  SdFs m_sd;

  cid_t m_cid;
  csd_t m_csd;

  uint32_t m_ocr;

  MbrSector_t m_mbr;

  FsFile m_logfile;
  FsFile m_meta;

  uint32_t m_logfile_size;

  RingBuf<FsFile, RING_BUF_CAPACITY> m_rbuf;

public:
  inline bool logging() const { return m_bLogging; }

  LoggerSD() :
    m_B(m_buftmp, 128),
    m_bLogging(false),
    m_bHaveMeta(false),
    m_config(FIFO_SDIO)
  {
    m_cat_owner.push(m_cat_task, true);
  }

  ~LoggerSD() {
    // ...
  }

  bool init(unsigned long *dt = 0, Shell *shell = 0) {
    uint32_t t0 = millis();

    if (!m_sd.cardBegin(m_config)) {
      if (shell) {
        *shell << "SD: initialization failed." << 0;
        if (isSpi(m_config)) {
          *shell << "SD is a SPI device - check CS pin, etc." << 0;
        }
      }
      return false;
    }
    if (dt)
      *dt = millis() - t0;

    if (!m_sd.card()->readCID(&m_cid) || !m_sd.card()->readCSD(&m_csd) || !m_sd.card()->readOCR(&m_ocr)) {
      if (shell)
        *shell << "SD: readInfo failed" << 0;
      return false;
    }

    if (!m_sd.card()->readSector(0, (uint8_t*) &m_mbr)) {
      if (shell)
        *shell << "SD: MBR read failed." << 0;
      return false;
    }

    return true;
  }
  inline bool init(Shell& shell) {
    return init(0, &shell);
  }

  bool cmd_sd_info(Shell& origin) {
    origin << "SdFat version: " << SD_FAT_VERSION_STR << 0;

    unsigned long dt;
    if (!init(&dt, &origin))
      return false;

    m_B.clear().printf("SD: init time: %ldms", dt);
    origin << m_B << 0;

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
    origin << "SD: card type: " << card_type << 0;

    m_B.clear().printf("SD: Manufacturer ID: %X; OEM ID: %c%c; Product: ", int(m_cid.mid), m_cid.oid[0], m_cid.oid[1]);
    for (uint8_t i = 0; i < 5; i++) {
      m_B.append(m_cid.pnm[i]);
    }
    m_B.printf("; Version: %d.%d", int(m_cid.prv_n), int(m_cid.prv_m));
    origin << m_B << 0;

    m_B.clear().printf("    Serial number: %lX; Manufacturing date: %d/%d; OCR: %lX",
		       m_cid.psn, int(m_cid.mdt_month), (2000 + 16*m_cid.mdt_year_high + m_cid.mdt_year_low), m_ocr);
    origin << m_B << 0;

    bool valid = true;

    origin << "SD: Partition Table" << 0;

    for (uint8_t ip = 1; ip < 5; ip++) {
      MbrPart_t *pt = &m_mbr.part[ip - 1];
      if ((pt->boot != 0 && pt->boot != 0x80) || getLe32(pt->relativeSectors) > sdCardCapacity(&m_csd)) {
        valid = false;
      }
      char boot = (pt->boot == 0) ? '-' : ((pt->boot == 0x80) ? '*' : '?');
      m_B.clear().printf("  %c %d %02x %03d:%03d:%03d %03d:%03d:%03d %10ld %10ld",
			 boot, ip, int(pt->type),
			 int(pt->beginCHS[0]), int(pt->beginCHS[1]), int(pt->beginCHS[2]),
			 int(pt->endCHS[0]), int(pt->endCHS[1]), int(pt->endCHS[2]),
			 getLe32(pt->relativeSectors), getLe32(pt->totalSectors));
      origin << m_B << 0;
    }
    if (!valid) {
      origin << "SD: MBR not valid, assuming Super Floppy format." << 0;
    }

    return true;    
  }

  bool cmd_ls(Shell& origin, const char *path = "/") {
    if (!init(origin))
      return false;

    if (!m_sd.volumeBegin()) {
      origin << "SD: Error! Filesystem error" << 0;
      return false;
    }

    if (!m_sd.exists(path)) {
      m_B.clear() << "SD: Error! '" << path << "' not found";
      origin << m_B << 0;
      return false;
    }

    FsFile file;
    if (!file.open(path, O_RDONLY)) {
      m_B.clear() << "SD: Error! Unable to read '" << path << "'";
      origin << m_B << 0;
      return false;
    }

    if (file.isDir()) {
      origin << "SD: Folder" << 0;
    } else {
      origin << "SD: File" << 0;
    }
    file.close();

    m_sd.ls(path, LS_DATE | LS_SIZE);

    return true;
  }

  bool cmd_mkdir(const char *path, Shell *shell) {
    if (!init(0, shell))
      return false;

    if (!m_sd.volumeBegin()) {
      if (shell)
        *shell << "SD: Error! Filesystem error" << 0;
      return false;
    }

    if (m_sd.exists(path)) {
      FsFile file;
      if (!file.open(path, O_RDONLY)) {
        if (shell) {
          m_B.clear() << "SD: Error! Unable to read '" << path << "'";
          *shell << m_B << 0;
        }
        return false;
      }
      bool bIsFolder = file.isDir();
      if (!bIsFolder && shell) {
        m_B.clear() << "SD: Error! '" << path << "' exists and is not a folder";
        *shell << m_B << 0;
      }
      file.close();
      return bIsFolder;
    }

    bool bCreated = m_sd.mkdir(path, true);

    if (!bCreated && shell) {
      m_B.clear() << "SD: Error! Unable to create '" << path << "'";
      *shell << m_B << 0;
    }
    return bCreated;
  }

  bool cmd_rm(Shell& origin, const char *path) {
    if (logging()) {
      origin << "SD: Logging is active; unable to delete file" << 0;
      return false;
    }

    if (!init(origin))
      return false;

    if (!m_sd.volumeBegin()) {
      origin << "SD: Error! Filesystem error" << 0;
      return false;
    }

    if (!m_sd.exists(path)) {
      m_B.clear() << "SD: Error! '" << path << "' not found";
      origin << m_B << 0;
      return false;
    }

    m_sd.remove(path);
 
    return true;
  }

  bool cmd_cat(Shell& origin, const char *path) {
    if (logging()) {
      origin << "SD: Logging is active; unable to cat file" << 0;
      return false;
    }

    if (!init(origin))
      return false;

    if (!m_sd.volumeBegin()) {
      origin << "SD: Error! Filesystem error" << 0;
      return false;
    }

    if (!m_sd.exists(path)) {
      m_B.clear() << "SD: Error! '" << path << "' not found";
      origin << m_B << 0;
      return false;
    }

    LoggerCatTask *tptr = m_cat_owner.pop();
    if (tptr) {
      if (tptr->assign(origin, m_B.clear(), path))
        origin << *tptr;
    } else {
      origin << "SD: Error! (busy)" << 0;
    }
    return true;
  }

  bool cmd_truncate(Shell& origin, const char *path) {
    if (logging()) {
      origin << "SD: Logging is active; unable to truncate file" << 0;
      return false;
    }

    if (!init(origin))
      return false;

    if (!m_sd.volumeBegin()) {
      origin << "SD: Error! Filesystem error" << 0;
      return false;
    }

    if (!m_sd.exists(path)) {
      m_B.clear() << "SD: Error! '" << path << "' not found";
      origin << m_B << 0;
      return false;
    }

    bool bMetaValid = false;
    unsigned long meta_length = 0;

    if (m_sd.exists("meta"))
      if (m_sd.chdir("meta")) {
        FsFile meta;
        if (meta.open(path, O_RDONLY))
          if (meta.size() == 15) {
            char buf[16];
            for (int i = 0; i < 15; i++)
              buf[i] = meta.read();
            buf[15] = 0;
            meta.close();
            if (sscanf(buf, "%lu", &meta_length) == 1) {
              bMetaValid = true;
            }
          }
        m_sd.chdir();
      }
    if (!bMetaValid) {
      origin << "SD: Error! Unable to read 'meta' data - unable to truncate" << 0;
      return false;
    }

    FsFile file;
    if (!file.open(path, O_RDWR)) {
      m_B.clear() << "SD: Error! Unable to open '" << path << "'";
      origin << m_B << 0;
      return false;
    }
    if (file.size() > meta_length) {
      origin << "SD: Truncating log file..." << 0;
      file.rewind();
      while (meta_length-- > 0)
        file.read();
      file.truncate();
    }
    file.close();
 
    return true;
  }

  bool cmd_log_start(const char *path, Shell *shell = 0) {
    if (m_bLogging) {
      if (shell)
        *shell << "Logging is already active." << 0;
      return false;
    }

    if (!init(0, shell))
      return false;

    if (!m_sd.volumeBegin()) {
      if (shell)
        *shell << "SD: Error! Filesystem error" << 0;
      return false;
    }

    if (!m_logfile.open(path, O_RDWR | O_CREAT | O_TRUNC)) {
      if (shell) {
	m_B.clear() << "Error! Failed to open " << path << " for writing.";
        *shell << m_B << 0;
      }
      return false;
    }

    if (!m_logfile.preAllocate(LOG_FILE_SIZE)) {
      if (shell)
        *shell << "Error! preAllocate failed" << 0;
      m_logfile.close();
      return false;
    }

    m_rbuf.begin(&m_logfile);

    m_bLogging = true;

    if (cmd_mkdir("meta", shell)) {
      m_sd.chdir("meta");

      if (m_meta.open(path, O_RDWR | O_CREAT | O_TRUNC)) {
        m_bHaveMeta = true;
      }
      m_sd.chdir();
    }
    m_logfile_size = 0;

    return true;
  }

  void cmd_log_stop(Shell *shell = 0) {
    if (!m_bLogging) {
      if (shell)
        *shell << "Logger is not active." << 0;
      return;
    }

    m_rbuf.sync();
    m_logfile.truncate();
    m_logfile.close();

    m_bLogging = false;

    if (m_bHaveMeta) {
      m_meta.close();
      m_bHaveMeta = false;
    }
  }

  void cmd_log_write(const ShellBuffer& buffer, Shell *shell = 0) {
    if (!m_bLogging)
      return;

    size_t n = m_rbuf.bytesUsed();
    if ((n + m_logfile.curPosition()) > (LOG_FILE_SIZE - 20)) {
      if (shell)
        *shell << "Logger: File full - quitting." << 0;
      cmd_log_stop();
      return;
    }

    m_rbuf.write(buffer.buffer(), buffer.count());

    if (m_rbuf.getWriteError()) {
      if (shell) {
        *shell << "Logger: write error (data too fast for logging)" << 0;
      }
    }
  }

  void cmd_log_tick(Shell *shell = 0) {
    if (!m_bLogging)
      return;

    if (m_rbuf.bytesUsed() >= 512 && !m_logfile.isBusy()) {
      if (512 != m_rbuf.writeOut(512)) {
        if (shell)
          *shell << "Logger: write error - quitting." << 0;
        cmd_log_stop();
      } else if (m_bHaveMeta) {
        m_logfile_size += 512;
        char buf[16];
        sprintf(buf, "%15lu", m_logfile_size);
        m_meta.rewind();
        m_meta.write(buf);
        m_meta.flush();
      }
    }
  }
};

#endif /* !CommaComms_LoggerSD_hh */
