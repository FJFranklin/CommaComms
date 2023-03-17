/* -*- mode: c++ -*-
 * 
 * Copyright 2020-22 Francis James Franklin
 * 
 * Open Source under the MIT License - see LICENSE in the project's root folder
 */

#include "ShellStream.hh"
#include "ShellBuffer.hh"

using namespace MultiShell;

#ifdef FEATHER_M0_BTLE
#include "Adafruit_BluefruitLE_SPI.h"

Adafruit_BluefruitLE_SPI ShellStream::m_bt_onboard(8, 7, 4);

bool ShellStream::check_connection() {
  if (m_bt->isConnected()) {
    if (!m_bConnected) {
      int count = m_bt_endptr - m_bt_bufptr;
      if (count) {
	m_bt_bufptr = 0;
	m_bt_endptr = 0;
      }

      m_bConnected = true;
      m_bt->sendCommandCheckOK("AT+HWModeLED=DISABLE");
      if (m_responder)
	m_responder->stream_notification(*this, "Connected");
    }
  } else {
    if (m_bConnected) {
      m_bConnected = false;
      m_bt->sendCommandCheckOK("AT+HWModeLED=MODE");
      if (m_responder)
	m_responder->stream_notification(*this, "Disconnected");
    }
  }
  return m_bConnected;
}

bool ShellStream::begin(const char *& status_str) { // For BTLE
  bool success = false;

  if (m_bt) {
    if (!m_bt->begin(false)) {
      status_str = "Failed to init BTLE!";
    } else if (!m_bt->factoryReset()) {
      status_str = "Failed to reset BTLE!";
    } else {
      m_bt->echo(false);
      // m_bt->info();
      // m_bt->verbose(false);
      m_bt->sendCommandCheckOK("AT+HWModeLED=MODE");

      status_str = "Init BTLE & reset - okay.";
      success = true;
    }
  } else {
    status_str = "Error: Not a BTLE stream.";
  }
  return success;
}
#endif

int ShellStream::read(ShellBuffer& buffer) {
  int count = 0;
  int space = buffer.space();
  if (space) {
    int afr = sync_read_begin();
    if (space < afr)
      afr = space;
    while (afr) {
      int value = read(afr);
      if (value < 0)
	break;
      buffer << (char) value;
      ++count;
    }
  }
  return count;
}

int ShellStream::sync_read_begin() {
  int count = 0;
#ifdef FEATHER_M0_BTLE
  if (m_bt) {
    if (m_bt_bufptr < m_bt_endptr) {
      count = m_bt_endptr - m_bt_bufptr;
    } else {
      if (check_connection()) { // nothing buffered, connection is up, let's check
	m_bt->println("AT+BLEUARTFIFO=RX");

	long bytes = m_bt->readline_parseInt();

	if (bytes < 1024) { // limit is 1024
	  count = 1024 - (int) bytes;
	}
	if (!m_bt->waitForOK()) {
	  if (m_responder)
	    m_responder->stream_notification(*this, "wait error (FIFO)");
	  count = 0;
	}
      }
      if (count) {
	m_bt->println("AT+BLEUARTRX");

	if (!m_bt->waitForOK()) {
	  if (m_responder)
	    m_responder->stream_notification(*this, "wait error (read)");
	  count = 0;
	} else {
	  m_bt_bufptr = m_bt->buffer;
	  m_bt_endptr = m_bt->buffer;
	  while (*m_bt_endptr) ++m_bt_endptr;
	  count = m_bt_endptr - m_bt_bufptr;
	}
      }
    }
  }
#endif
#if defined(TEENSYDUINO) || defined(ADAFRUIT_FEATHER_M0)
  if (m_usbser) {
    count = m_usbser->available();
  }
#endif
  if (m_serial) {
    count = m_serial->available();
  }
  return count;
}

int ShellStream::read(int& afr) {
  int value = -1;

  if (afr <= 0)
    return value;

#ifdef FEATHER_M0_BTLE
  if (m_bt) {
    if (m_bt_bufptr < m_bt_endptr) {
      value = *m_bt_bufptr++;
#if 0
      if (m_responder) {
	char buf[4] = {'{', value, '}', 0};
	m_responder->stream_notification(*this, buf);
      }
#endif
    }
  }
#endif
#if defined(TEENSYDUINO) || defined(ADAFRUIT_FEATHER_M0)
  if (m_usbser) {
    value = m_usbser->read();
  }
#endif
  if (m_serial) {
    value = m_serial->read();
  }
  if (value >= 0) {
    if (m_responder) {
      if (value == 4)
	m_responder->stream_notification(*this, "end");
      if (value == 6)
	m_responder->stream_notification(*this, "RSVP");
    }
    --afr;
  }
  return value;
}

int ShellStream::write_eol(int& afw) {
  if (afw < m_eol_length) {
    afw = 0;
    return 0;
  }

  const char *eolptr = m_eol;
  while (*eolptr && afw) {
    write_char(*eolptr++);
    --afw;
  }

  if (afw < m_eol_length)
    afw = 0;
  return m_eol_length;
}

int ShellStream::write(char c, int& afw) {
  if (!c || !afw || c == '\r')
    return 0;
  if (c == '\n')
    return write_eol(afw);

  write_char(c);

  if (--afw < m_eol_length)
    afw = 0;
  return 1;
}

void ShellStream::write_char(char c) {
#ifdef FEATHER_M0_BTLE
  if (m_bt) {
    m_bt->write(c);
  }
#endif
#if defined(TEENSYDUINO) || defined(ADAFRUIT_FEATHER_M0)
  if (m_usbser) {
    m_usbser->write(c);
  }
#endif
  if (m_serial) {
    m_serial->write(c);
  }
}

int ShellStream::sync_write_begin() {
  int count = 0;
#ifdef FEATHER_M0_BTLE
  if (m_bt) {
    if (check_connection()) { // connection is up, let's check
      m_bt->println("AT+BLEUARTFIFO=TX");

      long bytes = m_bt->readline_parseInt();

      if (bytes > 200) // limit single transaction length // AT commands limited to 240 bytes?? - check
	count = 200;
      else if (bytes)
	count = (int) bytes - 1;
      if (count < m_eol_length)
	count = 0;
      if (count)
	m_bt->print("AT+BLEUARTTX=");
    }
  }
#endif
#if defined(TEENSYDUINO) || defined(ADAFRUIT_FEATHER_M0)
  if (m_usbser) {
    count = m_usbser->availableForWrite();
    if (count < m_eol_length)
      count = 0;
  }
#endif
  if (m_serial) {
    count = m_serial->availableForWrite();
    if (count < m_eol_length)
      count = 0;
  }
  return count;
}

void ShellStream::sync_write_end() {
#ifdef FEATHER_M0_BTLE
  if (m_bt) {
    m_bt->println();
    if (!m_bt->waitForOK()) {
      if (m_responder)
	m_responder->stream_notification(*this, "wait error (write)");
    }
  }
#endif
}
