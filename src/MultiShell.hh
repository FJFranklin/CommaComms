#ifndef __MultiShell_hh__
#define __MultiShell_hh__

#include <Arduino.h>

#define MultiShell_BufferSize 64

class Shell;
class ShellHandler;
class ShellCommandList;

class ShellCommand {
public:
  const char *m_command;
  const char *m_usage;
  const char *m_description;

  ShellHandler *m_handler;
  ShellCommand *m_next;

  ShellCommand(const char *command, const char *usage, const char *description = 0) :
    m_command(command),
    m_usage(usage),
    m_description(description),
    m_handler(0),
    m_next(0)
  {
    // ...
  }
  ~ShellCommand() {
    // ...
  }
};

class ShellHandler {
public:
  class CommandArgs {
  private:
    char *m_arg;
    char  m_swap;

    void first();
    void next();

  public:
    CommandArgs(char *command_buffer) :
      m_arg(command_buffer),
      m_swap(0)
    {
      first();
    }
    ~CommandArgs() {
      // ...
    }
    inline CommandArgs& operator++() {
      next();
      return *this;
    }
    inline const char *c_str() const { return m_arg; }
  };

  enum CommandError
    {
     ce_Okay = 0,
     ce_IncorrectUsage,
     ce_UnhandledCommand,
     ce_OtherError
    };
  virtual CommandError shell_command(Shell& origin, CommandArgs& args) = 0;

  virtual ~ShellHandler() { }
};

inline bool operator==(ShellHandler::CommandArgs& lhs, const char *rhs) {
  return (strcmp(lhs.c_str(), rhs) == 0);
}
inline bool operator!=(ShellHandler::CommandArgs& lhs, const char *rhs) {
  return (strcmp(lhs.c_str(), rhs) != 0);
}

class Shell {
public:
  enum InputState {
    is_Start,
    is_Discard,
    is_Processing,
    is_String
  };
private:
  HardwareSerial   *m_serial;
  usb_serial_class *m_usbser;
  ShellCommandList *m_command_list;
  InputState        m_state;
  int               m_index;
  char              m_buffer[MultiShell_BufferSize];

  bool active() {
    if (m_serial)
      return *m_serial;
    return *m_usbser;
  }
  int read() { // returns -1 if no data available
    return m_serial ? m_serial->read() : m_usbser->read();
  }

public:
  Shell(usb_serial_class& serial, ShellCommandList& list) :
    m_serial(0),
    m_usbser(&serial),
    m_command_list(&list),
    m_state(is_Start),
    m_index(0)
  {
    // ...
  }
  Shell(HardwareSerial& serial, ShellCommandList& list) :
    m_serial(&serial),
    m_usbser(0),
    m_command_list(&list),
    m_state(is_Start),
    m_index(0)
  {
    // ...
  }
  ~Shell() {
    // ...
  }
  inline size_t write(const char *buffer, size_t size) {
    return m_serial ? m_serial->write(buffer, size) : m_usbser->write(buffer, size);
  }
  inline size_t write(const char *str) {
    return write(str, strlen(str));
  }
  inline size_t eol() {
    return write("\n", 1);
  }
  inline void triple(const char *one, const char *two, const char *three) {
    write(one);
    write(two);
    write(three);
  }
  void tick(); // input processing
};

class ShellCommandList : public ShellHandler {
private:
  ShellCommand  m_help;
  ShellHandler *m_default_handler;
  ShellCommand *m_list_first;
  ShellCommand *m_list_last;

public:
  ShellCommandList(ShellHandler *default_handler = 0) :
    m_help("help", "help", "List all commands and usage."),
    m_default_handler(default_handler)
  {
    m_help.m_handler = this;

    m_list_first = &m_help;
    m_list_last  = &m_help;
  }
  virtual ~ShellCommandList() {
    //
  }
  ShellHandler *default_handler() const {
    return m_default_handler;
  }
  void set_default_handler(ShellHandler *default_handler) {
    m_default_handler = default_handler;
  }
  virtual CommandError shell_command(Shell& origin, CommandArgs& args) {
    if (args != "help")
      return ce_UnhandledCommand;

    const ShellCommand *sc = m_list_first;

    while (sc) {
      origin.write(sc->m_usage);
      origin.eol();
      if (sc->m_description) {
	      origin.write("    ");
	      origin.write(sc->m_description);
	      origin.eol();
	    }
      sc = sc->m_next;
    }
    return ce_Okay;
  }
  void add(ShellCommand& command, ShellHandler *SH = 0) {
    command.m_handler = SH;

    m_list_last->m_next = &command;
    m_list_last = &command;
  }
  const ShellCommand *lookup(const char *command) const {
    const ShellCommand *sc = m_list_first;

    while (sc) {
      if (strcmp(sc->m_command, command) == 0)
	      break;
      sc = sc->m_next;
    }
    return sc;
  }
};

#endif /* !__MultiShell_hh__ */
