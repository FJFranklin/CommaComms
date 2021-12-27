/* Copyright 2019 Francis James Franklin
 * 
 * Open Source under the MIT License - see LICENSE in the project's root folder
 */

#include "CC_Serial.hh"

CC_Serial::CC_Serial(Stream & HS, char identifier, CommaComms::CC_Responder * R) :
  CommaComms(R),
  m_serial(&HS)
{
  m_id[0] = 's';
  m_id[1] = identifier;
  m_id[2] = 0;
}

CC_Serial::~CC_Serial() {
  // ...
}

const char * CC_Serial::name() const {
  return m_id;
}

void CC_Serial::update() {
  while (m_serial->available()) {
    push((char) m_serial->read());
  }
  while (m_serial->availableForWrite()) {
    char c;
    if (!m_fifo.pop(c)) break;
    m_serial->write(c);
  }
}

void CC_Serial::write(const char * str, bool add_eol) {
  if (str) {
    if (add_eol) {
      m_serial->println(str);
    } else {
      m_serial->print(str);
    }
  }
}
