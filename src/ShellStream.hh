/* -*- mode: c++ -*-
 * 
 * Copyright 2020-22 Francis James Franklin
 * 
 * Open Source under the MIT License - see LICENSE in the project's root folder
 */

#ifndef __ShellStream_hh__
#define __ShellStream_hh__

#include <ShellUtils.hh>

namespace MultiShell {

#ifdef FEATHER_M0_BTLE
  class Adafruit_BluefruitLE_SPI;
#endif

  class ShellBuffer;

  class ShellStream {
  public:
    class Responder {
    public:
      virtual void stream_notification(ShellStream& stream, const char *message) = 0;

      virtual ~Responder() { }
    };
  private:
    Responder *m_responder;

    const char *m_eol;
    int         m_eol_length;

#ifdef FEATHER_M0_BTLE
    const char *m_bt_bufptr;
    const char *m_bt_endptr;
    bool        m_bConnected;

    Adafruit_BluefruitLE_SPI *m_bt;
  public:
    static Adafruit_BluefruitLE_SPI m_bt_onboard;
  private:
#endif
#if defined(TEENSYDUINO)
    usb_serial_class *m_usbser;
#endif
#if defined(ADAFRUIT_FEATHER_M0)
    Serial_          *m_usbser;
#endif
#ifdef OS_Linux
    VirtualSerial    *m_serial;
#else
    HardwareSerial   *m_serial;
#endif

    char  m_name[3];

    void set_name(char stream_type, char identifier) {
      m_name[0] = stream_type;
      m_name[1] = identifier;
      m_name[2] = 0;
    }

  public:
    inline const char *name() const {
      return m_name;
    }
    inline void set_responder(Responder *responder) {
      m_responder = responder;
    }
    inline void set_eol(const char *eol_str) {
      if (eol_str) {
	m_eol = eol_str;
	m_eol_length = strlen(eol_str);
      }
    }

#ifdef OS_Linux
    ShellStream(VirtualSerial& serial, char identifier = '?') :
      m_responder(0),
      m_serial(&serial)
    {
      set_name('v', identifier);
      set_eol("\n");
    }
#else
    ShellStream(HardwareSerial& serial, char identifier = '?') :
      m_responder(0),
#ifdef FEATHER_M0_BTLE
      m_bt_bufptr(0),
      m_bt_endptr(0),
      m_bConnected(false),
      m_bt(0),
#endif
#if defined(TEENSYDUINO) || defined(ADAFRUIT_FEATHER_M0)
      m_usbser(0),
#endif
      m_serial(&serial)
    {
      set_name('s', identifier);
      set_eol("\n");
    }
#endif
#if defined(TEENSYDUINO)
    ShellStream(usb_serial_class& serial, char identifier = '?') :
      m_responder(0),
      m_usbser(&serial),
      m_serial(0)
    {
      set_name('u', identifier);
      set_eol("\n");
    }
#endif
#if defined(ADAFRUIT_FEATHER_M0)
    ShellStream(Serial_& serial, char identifier = '?') :
      m_responder(0),
#ifdef FEATHER_M0_BTLE
      m_bt_bufptr(0),
      m_bt_endptr(0),
      m_bConnected(false),
      m_bt(0),
#endif
      m_usbser(&serial),
      m_serial(0)
    {
      set_name('u', identifier);
      set_eol("\n");
    }
#endif
#ifdef FEATHER_M0_BTLE
    ShellStream(Adafruit_BluefruitLE_SPI& bt, char identifier = '?') :
      m_responder(0),
      m_bt_bufptr(0),
      m_bt_endptr(0),
      m_bConnected(false),
      m_bt(&bt),
      m_usbser(0),
      m_serial(0)
    {
      set_name('b', identifier);
      set_eol("\\n");
    }
#endif
    ~ShellStream() {
      // ...
    }

#ifdef FEATHER_M0_BTLE
    bool check_connection();
#endif

    inline operator bool() {
      if (m_serial)
	return *m_serial;
#if defined(TEENSYDUINO)
      if (m_usbser)
	return *m_usbser;
#endif
#if defined(ADAFRUIT_FEATHER_M0)
      if (m_usbser)
	return m_usbser->dtr();
      // Serial_::operator bool() adds a 10ms delay (!!) for reasons:
      // "We add a short delay before returning to fix a bug observed by Federico
      //  where the port is configured (lineState != 0) but not quite opened."
#endif
#ifdef FEATHER_M0_BTLE
      if (m_bt)
	return check_connection();
#endif
      return false;
    }

    int  read(ShellBuffer& buffer);

    int  sync_read_begin();       // returns afr = available()
    int  read(int& afr);

    int  sync_write_begin();      // returns afw = availableForWrite()
    void sync_write_end();

    int  write_eol(int& afw);     // these write functions track afw, which should
    int  write(char c, int& afw); // always be sufficient to write an EOL

    inline void update() {
#ifdef OS_Linux
      m_serial->update();
#endif
    }
  private:
    void write_char(char c);
  public:

#ifdef OS_Linux
    bool begin(const char *& status_str, unsigned long baud = 0) {
      return m_serial->begin(status_str, baud);
    }
#else
#ifdef FEATHER_M0_BTLE
    bool begin(const char *& status_str); // For BTLE only
#endif
    void begin(unsigned long baud, unsigned long timeout_ms = 100) {
      unsigned long t0 = millis();
      while (millis() < t0 + timeout_ms) {
	if (*this) break;
      }
      if (m_serial)
	m_serial->begin(baud);
#if defined(TEENSYDUINO) || defined(ADAFRUIT_FEATHER_M0)
      else if (m_usbser)
	m_usbser->begin(baud);
#endif
    }
#endif
  };

} // MultiShell

#endif /* !__ShellStream_hh__ */
