/* -*- mode: c++ -*-
 * 
 * Copyright 2022 Francis James Franklin
 * 
 * Open Source under the MIT License - see LICENSE in the project's root folder
 */

#include <string>

#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <fcntl.h>

#include <ShellExtra.hh>

namespace MultiShell {

  unsigned long millis() {
    unsigned long us = 0;

    static bool bInit = false;

    static unsigned long time_secs = 0;
    static unsigned long time_nano = 0;

    struct timespec ts;
    clock_gettime (CLOCK_MONOTONIC, &ts);
    unsigned long new_time_secs = (unsigned long) ts.tv_sec;
    unsigned long new_time_nano = (unsigned long) ts.tv_nsec;

    if (!bInit) {
      time_secs = new_time_secs;
      time_nano = new_time_nano;
      bInit = true;
    } else {
      if (new_time_nano < time_nano) {
	us  = (1000000000UL + new_time_nano - time_nano) / 1000000UL;
	us += 1000UL * (new_time_secs - time_secs - 1);
      } else {
	us  = (new_time_nano - time_nano) / 1000000UL;
	us += 1000UL * (new_time_secs - time_secs);
      }
    }
    return us;
  }

} // MultiShell

using namespace MultiShell;

void s_sync_read(FIFO& in, int fd) {
  int afw = in.availableForWrite();
  if (!afw) // FIFO is full
    return;

  struct timeval tv;

  tv.tv_sec = 0;
  tv.tv_usec = 0;

  fd_set fdset;

  FD_ZERO(&fdset);
  FD_SET(fd, &fdset);

  while (afw--) {
    select(fd + 1, &fdset, 0, 0, &tv);

    if (!FD_ISSET(fd, &fdset)) // no input
      break;

    char c;
    read(fd, &c, 1);
    in.push(c);
  }
}

void s_sync_write(FIFO& out, int fd) {
  int afr = out.available();
  if (!afr) // FIFO is empty
    return;

  struct timeval tv;

  tv.tv_sec = 0;
  tv.tv_usec = 0;

  fd_set fdset;

  FD_ZERO(&fdset);
  FD_SET(fd, &fdset);

  while (afr--) {
    if (select(fd + 1, 0, &fdset, 0, &tv) == -1) {
      // error!
      break;
    }

    if (!FD_ISSET(fd, &fdset)) // can't output
      break;

    char c;
    if (!out.pop(c))
      break;
    write(fd, &c, 1);
  }
}

Terminal::Terminal() {
  tcgetattr(STDIN_FILENO, &m_ttysave);
}

Terminal::~Terminal() {
  /* probably unnecessary - restore terminal
   */
  tcsetattr(STDIN_FILENO, TCSANOW, &m_ttysave);
}

bool Terminal::begin(const char *& status, unsigned long baud) {
  struct termios ttystate = m_ttysave;

  ttystate.c_lflag &= ~(ICANON /*| ECHO*/); // TODO: Check
  ttystate.c_cc[VMIN] = 1;

  if (tcsetattr(STDIN_FILENO, TCSANOW, &ttystate)) {
    status = "VirtualSerial: Terminal: Unable to set serial attributes correctly.";
    m_bActive = false;
  } else {
    m_bActive = true;
  }
  return m_bActive;
}

void Terminal::sync_read() {
/*s_sync_read(m_in, fileno(stdin));
  return;*/

  int afw = m_in.availableForWrite();
  if (!afw) // FIFO is full
    return;

  struct timeval tv;

  tv.tv_sec = 0;
  tv.tv_usec = 0;

  fd_set fdset;

  FD_ZERO(&fdset);
  FD_SET(fileno(stdin), &fdset);

  while (afw--) {
    select(fileno(stdin) + 1, &fdset, 0, 0, &tv);

    if (!FD_ISSET(fileno(stdin), &fdset)) // no input
      break;

    char c = fgetc(stdin);
    m_in.push(c);
  }
}

void Terminal::sync_write() {
/*s_sync_write(m_out, fileno(stdout));
  return;*/

  int afr = m_out.available();
  if (!afr) // FIFO is empty
    return;

  struct timeval tv;

  tv.tv_sec = 0;
  tv.tv_usec = 0;

  fd_set fdset;

  FD_ZERO(&fdset);
  FD_SET(fileno(stdout), &fdset);

  while (afr--) {
    if (select(fileno(stdout) + 1, 0, &fdset, 0, &tv) == -1) {
      // error!
      break;
    }

    if (!FD_ISSET(fileno(stdout), &fdset)) // can't output
      break;

    char c;
    if (!m_out.pop(c))
      break;
    fputc(c, stdout);
  }
}

GenericSerial::GenericSerial(const char *device) :
  m_device(device),
  m_fd(-1)
{
  // ...
}

GenericSerial::~GenericSerial() {
  if (m_fd > -1) {
    close(m_fd);
  }
}

bool GenericSerial::begin(const char *& status, unsigned long baud) {
  /* set up serial
   */
  m_fd = open(m_device, O_RDWR | O_NOCTTY | O_NONBLOCK /* O_NDELAY */);
  if (m_fd == -1) {
    status = "VirtualSerial: GenericSerial: Failed to open device - exiting.";
    return false;
  }

  struct termios options;

  tcgetattr(m_fd, &options);

  options.c_cflag = CS8 | CLOCAL | CREAD;
  options.c_iflag = IGNPAR;
  options.c_oflag = 0;
  options.c_lflag = 0;

  cfsetispeed(&options, B115200);
  cfsetospeed(&options, B115200);

  tcflush(m_fd, TCIFLUSH);
  tcsetattr(m_fd, TCSANOW, &options);

  m_bActive = true;
  return m_bActive;
}

void GenericSerial::sync_read() {
  s_sync_read(m_in, m_fd);
}

void GenericSerial::sync_write() {
  s_sync_write(m_out, m_fd);
}
