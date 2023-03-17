/* -*- mode: c++ -*-
 * 
 * Copyright 2022 Francis James Franklin
 * 
 * Open Source under the MIT License - see LICENSE in the project's root folder
 */

#ifndef __Shell_hh__
#define __Shell_hh__

#include <ShellUtils.hh>
#include <CommaComms.hh>
#include <ShellStream.hh>
#include <ShellBuffer.hh>
#include <ShellCommand.hh>
#include <ShellOption.hh>
#include <ShellTask.hh>
#include <ShellPlot.hh>

namespace MultiShell {

  class Shell : public Dispatcher {
  private:
    static Repository  m_repository;

    static const int  BufferSize = 64;

    TaskOwner<Task>  m_manager;

    CommandList  *m_command_list;
    Comma         m_comma;
    ShellStream  *m_stream;
    ShellHandler *m_handler;
    InputState    m_state;

    int   m_index;
    char  m_buffer[BufferSize];
    char  m_name[3];

    inline void reset(InputState is = is_CC) {
      m_state = is;
      m_index = 0;
    }
    inline void set_name(char identifier) {
      m_name[0] = 'm';
      m_name[1] = identifier;
      m_name[2] = 0;
    }
  public:
    static inline void repository_status(ShellBuffer& buffer) {
      m_repository.status(buffer);
    }
    static inline ShellBuffer *tmp_buffer() { // get a temporary 128-byte buffer
      return m_repository.tmp_buffer();
    }

    inline const char *name() const {
      return m_name;
    }

    Shell(ShellStream& stream, CommandList& list, char id) :
      m_command_list(&list),
      m_stream(&stream),
      m_handler(0)
    {
      reset();
      set_name(id);
      init(m_manager, m_repository);
    }
    virtual ~Shell();

    inline void set_handler(ShellHandler *handler) {
      m_handler = handler;
    }
    inline ShellHandler *handler() const { return m_handler; }

    inline void respond_to_RSVP() {
      m_manager.respond_to_RSVP();
    }

    void update();
  };
  inline Dispatcher& operator<<(Dispatcher& lhs, const Args& args) {
    lhs.dispatch_buffer(args.c_str(), strlen(args.c_str()));
    return lhs;
  }

} // MultiShell

#endif /* !__Shell_hh__ */
