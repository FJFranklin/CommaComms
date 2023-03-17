/* Copyright 2019-22 Francis James Franklin
 * 
 * Open Source under the MIT License - see LICENSE in the project's root folder
 */

#ifndef cariot_CommaComms_hh
#define cariot_CommaComms_hh

#include <ShellUtils.hh>
#include <ShellBuffer.hh>

namespace MultiShell {

  class CommaCommand {
  public:
    char          m_command;
    unsigned long m_value;

    CommaCommand(char command = 0, unsigned long value = 0) :
      m_command(command),
      m_value(value)
    {
      // ...
    }
    ~CommaCommand() {
      // ...
    }
    ShellBuffer& append(ShellBuffer& buffer) const {
      buffer.printf("%c%lu,", m_command, m_value);
      return buffer;
    }
  };

  class Comma {
  public:
    static float unpack754_32(uint32_t i);
    static uint32_t pack754_32(float f);

  private:
    int  m_length;
    char m_buffer[16]; // command receive buffer
  public:
    Comma() : m_length(0) {
      // ...
    }
    ~Comma() {
      // ...
    }

    bool push(char c, CommaCommand& C); // returns true & updates C if a command is received
  };

} // MultiShell

#endif /* !cariot_CommaComms_hh */
