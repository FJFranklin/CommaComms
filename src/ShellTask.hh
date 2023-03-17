/* -*- mode: c++ -*-
 * 
 * Copyright 2020-22 Francis James Franklin
 * 
 * Open Source under the MIT License - see LICENSE in the project's root folder
 */

#ifndef __ShellTask_hh__
#define __ShellTask_hh__

#include <ShellUtils.hh>
#include <CommaComms.hh>

namespace MultiShell {

  class ShellStream;
  class TaskList;

  class Task : public LinkedItem {
    friend TaskList;
  private:
    static const unsigned char fMaskEOL  = 0x03;
    static const unsigned char fMaskRSVP = 0x70;

    unsigned char  m_flags;
  public:
    Task() : m_flags(0)
    {
      // ...
    }
    virtual ~Task();

    virtual bool process_task(ShellStream& stream, int& afw); // returns true on completion of task
  private:
    inline void push_eol() {
      unsigned char eol_count = m_flags & fMaskEOL;
      if (eol_count < 3) {
	m_flags &= ~fMaskEOL;
	m_flags |= ++eol_count;
      }
    }
  };

  class TaskList : public LinkedList {
  private:
    unsigned char  m_flags;
  public:
    TaskList() : m_flags(0) {
      // ...
    }
    virtual ~TaskList();
  protected:
    virtual void linked_item_push(LinkedItem& item);
  public:
    inline void push_eol() {
      Task *tptr = (Task *) linked_item(count()-1);
      if (tptr) {
	tptr->push_eol();
      } else {
	unsigned char eol_count = m_flags & Task::fMaskEOL;
	if (eol_count < 3) {
	  m_flags &= ~Task::fMaskEOL;
	  m_flags |= ++eol_count;
	}
      }
    }
    inline void respond_to_RSVP() {
      Task *tptr = (Task *) linked_item(count()-1);
      if (tptr) {
        tptr->m_flags |= Task::fMaskRSVP;
      } else {
        m_flags |= Task::fMaskRSVP;
      }
    }
    void process_tasks(ShellStream& stream);
  };

  template<class T> class TaskOwner : public TaskList {
  public:
    virtual ~TaskOwner() { }

    inline void push(T& item, bool bAdopt = false) {
      if (bAdopt)
	linked_item_adopt(item);
      linked_item_push(item);
    }
    inline T *pop() {
      return (T *) linked_item_pop();
    }
    inline const T *item(int index) const {
      return (const T *) linked_item(index);
    }
  };

  class Task_OffsetString : public Task {
  private:
    const char *m_str;
    int         m_offset;
  public:
    Task_OffsetString() : m_str(0), m_offset(0) {
      // ...
    }
    virtual ~Task_OffsetString();

    inline void assign(const char *str, int offset = 0) {
      m_str = str;
      m_offset = (offset < 0) ? 0 : offset;
    }
    virtual bool process_task(ShellStream& stream, int& afw); // returns true on completion of task
  };

  class Task_Buffer : public Task {
  private:
    char *m_buffer;
    char *m_bufend;
    char *m_bufptr;
  public:
    Task_Buffer() : m_buffer(0), m_bufend(0), m_bufptr(0) {
      // ...
    }
    virtual ~Task_Buffer();

    inline void init(char *buffer, int length) {
      if (buffer && length > 0) {
	m_buffer = buffer;
	m_bufend = buffer + length;
	m_bufptr = m_bufend; // empty
      }
    }
    inline int capacity() const {
      return m_bufend - m_buffer;
    }
    inline int count() const {
      return m_bufend - m_bufptr;
    }
    int assign(const char *buffer, int length);

    virtual bool process_task(ShellStream& stream, int& afw); // returns true on completion of task
  };

  class Task_Printable : public Task {
  private:
    const PrintableList *m_list;
    const PrintableItem *m_item;
    const char  *m_ptr;
    int  m_offset;
    int  m_iindex;
    int  m_pindex;
  public:
    Task_Printable() :
      m_list(0),
      m_item(0),
      m_ptr(0),
      m_offset(0),
      m_iindex(-1),
      m_pindex(-1)
    {
      // ...
    }
    virtual ~Task_Printable();

    inline void assign(const PrintableList& list) {
      m_list = &list;
      m_item = 0;
      m_ptr = 0;
      m_iindex = -1;
      m_pindex = -1;
    }

    virtual bool process_task(ShellStream& stream, int& afw); // returns true on completion of task
  };

  class Task_Comma : public Task {
  private:
    unsigned long  m_value;
    char     m_command;
    uint8_t  m_digits;
  public:
    Task_Comma() :
      m_value(0),
      m_command(0),
      m_digits(0)
    {
      // ...
    }
    virtual ~Task_Comma();

    inline void assign(const CommaCommand& C) {
      m_value   = C.m_value;
      m_command = C.m_command;
      m_digits  = 0;

      if (m_value) {
	m_digits = 1;
	unsigned long divisor = 1;
	while (true) {
	  if (m_value / divisor < 10)
	    break;
	  ++m_digits;
	  divisor *= 10;
	}
      }
    }

    virtual bool process_task(ShellStream& stream, int& afw); // returns true on completion of task
  };

  class Repository {
  private:
    char m_buf_gp[4][128];

    char m_buf_16[256];
    char m_buf_32[256];
    char m_buf_64[256];

    ShellBuffer       m_gpbuf[4];

    Task_Buffer       m_tasks[28];
    Task_OffsetString m_ostrs[16];
    Task_Printable    m_plist[2];
    Task_Comma        m_comma[16];

    ItemOwner<ShellBuffer>       m_owner_gp; // general purpose buffers

    TaskOwner<Task_OffsetString> m_owner_os; // offset strings
    TaskOwner<Task_Printable>    m_owner_pl; // printable lists
    TaskOwner<Task_Comma>        m_owner_cc; // comma commands
    TaskOwner<Task_Buffer>       m_owner_16; // buffers of size 16
    TaskOwner<Task_Buffer>       m_owner_32; // buffers of size 32
    TaskOwner<Task_Buffer>       m_owner_64; // buffers of size 64
  public:
    Repository();

    ~Repository() {
      // ...
    }

    /** returns pointer to a 128-byte buffer for temporary use; use B->return_to_owner() to free
     */
    inline ShellBuffer *tmp_buffer() {
      ShellBuffer *B = m_owner_gp.pop();
      if (B)
	B->clear();
      return B;
    }

    inline bool dispatch_offset_string(TaskOwner<Task>& manager, const char *str, unsigned offset = 0) {
      if (!str) {
	manager.push_eol();
	return true;
      }
      Task_OffsetString *tptr = m_owner_os.pop();
      if (tptr) {
	tptr->assign(str, offset);
	manager.push(*tptr);
	return true;
      }
      return false;
    }

    bool dispatch_buffer(TaskOwner<Task>& manager, const char *buffer, unsigned length);

    inline bool dispatch_buffer(TaskOwner<Task>& manager, const ShellBuffer& buffer) {
      return dispatch_buffer(manager, buffer.buffer(), (unsigned) buffer.count());
    }
    inline bool dispatch_command(TaskOwner<Task>& manager, const CommaCommand& command) {
      if (command.m_command) {
	Task_Comma *tptr = m_owner_cc.pop();
	if (tptr) {
	  tptr->assign(command);
	  manager.push(*tptr);
	  return true;
	}
      }
      return false;
    }

    inline bool dispatch_printable_list(TaskOwner<Task>& manager, const PrintableList& list) {
      Task_Printable *tptr = m_owner_pl.pop();
      if (tptr) {
	tptr->assign(list);
	manager.push(*tptr);
	return true;
      }
      return false;
    }

    void status(ShellBuffer& buffer);
  };

  class Dispatcher {
  private:
    TaskOwner<Task> *m_manager;
    Repository      *m_repository;
  public:
    Dispatcher(TaskOwner<Task>& manager, Repository& repository) :
      m_manager(&manager),
      m_repository(&repository)
    {
      // ...
    }
  protected:
    Dispatcher() :
      m_manager(0),
      m_repository(0)
    {
      // ...
    }
    inline void init(TaskOwner<Task>& manager, Repository& repository) {
      m_manager = &manager;
      m_repository = &repository;
    }
  public:
    virtual ~Dispatcher();

    inline bool dispatch_offset_string(const char *str, unsigned offset = 0) {
      return m_repository->dispatch_offset_string(*m_manager, str, offset);
    }
    inline bool dispatch_buffer(const char *buffer, unsigned length) {
      if (buffer && length)
	if (*buffer)
	  return m_repository->dispatch_buffer(*m_manager, buffer, length);
      return false;
    }
    inline bool dispatch_buffer(const ShellBuffer& buffer) {
      return m_repository->dispatch_buffer(*m_manager, buffer);
    }
    inline bool dispatch_command(const CommaCommand& command) {
      return m_repository->dispatch_command(*m_manager, command);
    }
    inline bool dispatch_printable_list(const PrintableList& list) {
      return m_repository->dispatch_printable_list(*m_manager, list);
    }
    inline void add_task(Task& task) {
      return m_manager->push(task);
    }
  };
  inline Dispatcher& operator<<(Dispatcher& lhs, const char *rhs) {
    lhs.dispatch_offset_string(rhs);
    return lhs;
  }
  inline Dispatcher& operator<<(Dispatcher& lhs, const ShellBuffer& buffer) {
    lhs.dispatch_buffer(buffer);
    return lhs;
  }
  inline Dispatcher& operator<<(Dispatcher& lhs, const CommaCommand& command) {
    lhs.dispatch_command(command);
    return lhs;
  }
  inline Dispatcher& operator<<(Dispatcher& lhs, const PrintableList& list) {
    lhs.dispatch_printable_list(list);
    return lhs;
  }
  inline Dispatcher& operator<<(Dispatcher& lhs, Task& task) {
    lhs.add_task(task);
    return lhs;
  }

} // MultiShell

#endif /* !__ShellTask_hh__ */
