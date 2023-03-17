/* -*- mode: c++ -*-
 * 
 * Copyright 2022 Francis James Franklin
 * 
 * Open Source under the MIT License - see LICENSE in the project's root folder
 */

#ifndef __ShellExtra_hh__
#define __ShellExtra_hh__

#include <ShellUtils.hh>

#include <termios.h>

namespace MultiShell {

  class Terminal : public VirtualSerial {
  private:
    struct termios  m_ttysave;

  public:
    Terminal();

    virtual ~Terminal();

    virtual bool begin(const char *&status, unsigned long baud);

    virtual void sync_read();
    virtual void sync_write();
  };

  class GenericSerial : public VirtualSerial {
  private:
    const char *m_device;
    int  m_fd;

  public:
    GenericSerial(const char *device);

    virtual ~GenericSerial();

    virtual bool begin(const char *&status, unsigned long baud);

    virtual void sync_read();
    virtual void sync_write();
  };

} // MultiShell

#endif /* !__ShellExtra_hh__ */
