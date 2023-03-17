/* -*- mode: c++ -*-
 * 
 * Copyright 2022 Francis James Franklin
 * 
 * Open Source under the MIT License - see LICENSE in the project's root folder
 */

#ifndef __ShellCommand_hh__
#define __ShellCommand_hh__

#include <ShellUtils.hh>

namespace MultiShell {

  class Shell;

  class Args {
  private:
    char *m_arg;
    char  m_swap;

    void first();
    void next();

  public:
    Args(char *command_buffer) :
      m_arg(command_buffer),
      m_swap(0)
    {
      first();
    }
    ~Args() {
      // ...
    }
    inline Args& operator++() {
      next();
      return *this;
    }
    inline const char *c_str() const { return m_arg; }
  };

  inline bool operator==(Args& lhs, const char *rhs) {
    return (strcmp(lhs.c_str(), rhs) == 0);
  }
  inline bool operator!=(Args& lhs, const char *rhs) {
    return (strcmp(lhs.c_str(), rhs) != 0);
  }

  class ShellHandler {
  public:
    virtual void shell_notification(Shell& origin, const char *message);
    virtual void comma_command(Shell& origin, CommaCommand& command);
    virtual CommandError shell_command(Shell& origin, Args& args);

    virtual ~ShellHandler() { }
  };

  class Command : public PrintableItem {
  private:
    const char *m_command;
    const char *m_usage;
    const char *m_description;

    ShellHandler *m_handler;
  public:
    inline const char *command() const { return m_command; }
    inline const char *usage() const { return m_usage; }
    inline const char *description() const { return m_description; }

    inline void set_handler(ShellHandler *handler) {
      m_handler = handler;
    }
    inline ShellHandler *handler() const { return m_handler; }

    Command(const char *command, const char *usage, const char *description = 0) :
      m_command(command),
      m_usage(usage),
      m_description(description),
      m_handler(0)
    {
      // ...
    }

    virtual ~Command();

    /* PrintableItem:
     */
    virtual int printable_count() const;
    virtual const char *printable(int index, int& offset) const;
  };

  class CommandList : public ShellHandler, public PrintableList {
  private:
    Command       m_help;
    Command       m_RSVP;
    ShellHandler *m_default_handler;

  public:
    CommandList(ShellHandler *default_handler = 0) :
      m_help("help", "help", "List all commands and usage."),
      m_RSVP("RSVP", "RSVP", "Send acknowledgement (ASCII Code 6 = ACK)."),
      m_default_handler(default_handler)
    {
      m_help.set_handler(this);
      m_RSVP.set_handler(this);

      push(m_help);
      push(m_RSVP);
    }
    virtual ~CommandList();

    ShellHandler *default_handler() const {
      return m_default_handler;
    }
    void set_default_handler(ShellHandler *default_handler) {
      m_default_handler = default_handler;
    }

    virtual CommandError shell_command(Shell& origin, Args& args);

    inline const Command *operator[](int index) const {
      return (const Command *) item(index);
    }

    inline void add(Command& command, ShellHandler *handler = 0) {
      command.set_handler(handler);
      push(command);
    }
    const Command *lookup(const char *command) const {
      const Command *match = 0;

      for (int i = 0; i < count(); i++) {
	const Command *sc = (const Command *) item(i);
	if (!sc) // shouldn't happen
	  break;

	const char *item_command = sc->command();
	if (!item_command) // shouldn't happen
	  continue;

	if (strcmp(item_command, command) == 0) {
	  match = sc;
	  break;
	}
      }
      return match;
    }

    /* PrintableList:
     * use defaults, i.e., no list-specific printables, no selection
     */
  };

} // MultiShell

#endif /* !__ShellCommand_hh__ */
