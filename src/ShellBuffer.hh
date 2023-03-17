/* -*- mode: c++ -*-
 * 
 * Copyright 2022 Francis James Franklin
 * 
 * Open Source under the MIT License - see LICENSE in the project's root folder
 */

#ifndef __ShellBuffer_hh__
#define __ShellBuffer_hh__

#include <ShellUtils.hh>

namespace MultiShell {

  class ShellBuffer : public LinkedItem {
  private:
    char *m_buffer;
    char *m_strend;

    const char *m_bufend;

    static char m_tmp[16]; // Do not use; part of a linker hack
    static bool m_bInit;

    static void init();
  
  public:
    void init(char *buffer, int length) {
      m_buffer = buffer;
      m_strend = buffer;
      m_bufend = buffer + length;
    }

    ShellBuffer() :
      m_buffer(0),
      m_strend(0),
      m_bufend(0)
    {
      if (m_bInit) init();
    }
    ShellBuffer(char *buffer, int length) :
      m_buffer(buffer),
      m_strend(buffer),
      m_bufend(buffer + length)
    {
      if (m_bInit) init();
    }
    ~ShellBuffer() {
      // ...
    }
    inline const char *c_str() {
      if (m_strend < m_bufend)
	*m_strend = 0;
      else
	*--m_strend = 0;
      return m_buffer;
    }
    inline const char *buffer() const {
      return m_buffer;
    }
    inline int count() const {
      return m_strend - m_buffer;
    }
    inline int space() const {
      return m_bufend - m_strend;
    }
    inline ShellBuffer& clear() {
      m_strend = m_buffer;
      return *this;
    }
    inline ShellBuffer& operator=(const char *str) {
      m_strend = m_buffer;
      if (str)
	if (*str) {
	  int length = strlen(str);
	  int lenmax = m_bufend - m_buffer;
	  if (length > lenmax)
	    length = lenmax;
	  memcpy(m_buffer, str, length);
	  m_strend += length;
	}
      return *this;
    }
    int printf(const char *fmt, ...) {
      if (m_bufend <= m_strend) return 0;
 
      va_list ap;
      va_start(ap, fmt);
      int result = vsnprintf(m_strend, m_bufend - m_strend - 1, fmt, ap);
      va_end(ap);

      m_strend += result;
      return result;
    }
    int append(const char *str) {
      int count = 0;

      if (str) {
	while (*str && m_strend < m_bufend) {
	  *m_strend++ = *str++;
	  ++count;
	}
      }
      return count;
    }
    int append(char c) {
      int count = 0;

      if (m_strend < m_bufend) {
	*m_strend++ = c;
	count = 1;
      }
      return count;
    }
  };

  inline ShellBuffer& operator<<(ShellBuffer& lhs, const char *rhs) {
    lhs.append(rhs);
    return lhs;
  }
  inline ShellBuffer& operator<<(ShellBuffer& lhs, char rhs) {
    lhs.append(rhs);
    return lhs;
  }

} // MultiShell

#endif /* !__ShellBuffer_hh__ */
