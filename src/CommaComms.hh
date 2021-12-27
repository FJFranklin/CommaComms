/* Copyright 2019-21 Francis James Franklin
 * 
 * Open Source under the MIT License - see LICENSE in the project's root folder
 */

#ifndef cariot_CommaComms_hh
#define cariot_CommaComms_hh

#include <Arduino.h>
#include "CC_FIFO.hh"

class CommaComms {
public:
  static float unpack754_32(uint32_t i);
  static uint32_t pack754_32(float f);

  class CC_Responder {
  public:
    virtual void notify(CommaComms * C, const char * str) = 0;
    virtual void command(CommaComms * C, char code, unsigned long value) = 0;

    virtual ~CC_Responder() { }
  };

protected:
  CC_Responder * m_Responder;
  CC_FIFO<char> m_fifo;

private:
  int m_length;
  char m_buffer[16]; // command receive buffer
  bool m_bUI;        // UI text mode
  bool m_bSOL;       // Start of line

public:
  CommaComms(CC_Responder * R);
  virtual ~CommaComms();
  virtual const char * name() const; // override this method to provide ID
  virtual void update();             // override this method to manage IO streams

  virtual void command_send(char code, unsigned long value = 0);
  virtual void command_print(const char * str);
  virtual void ui(char c = 0);
  void ui_print(const char * str) {
    if (str)
      while(*str) {
        ui(*str++);
      }
  }

  virtual const char * eol();

protected:
  void notify(const char * str);
  void command(char code, unsigned long value);
  void push(char c);
};

#endif /* !cariot_CommaComms_hh */
