/* Copyright 2019 Francis James Franklin
 * 
 * Open Source under the MIT License - see LICENSE in the project's root folder
 */

#ifndef cariot_CC_Serial_hh
#define cariot_CC_Serial_hh

#include "CommaComms.hh"

class CC_Serial : public CommaComms {
private:
  Stream * m_serial;

  char m_id[3];

public:
  CC_Serial(Stream & HS, char identifier, CommaComms::CC_Responder * R);

  virtual ~CC_Serial();

  virtual const char * name() const;
  virtual void update();
  virtual void write(const char * str, bool add_eol);
};

#endif /* !cariot_CC_Serial_hh */
