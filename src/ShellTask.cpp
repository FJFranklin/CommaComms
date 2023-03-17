/* -*- mode: c++ -*-
 * 
 * Copyright 2020-22 Francis James Franklin
 * 
 * Open Source under the MIT License - see LICENSE in the project's root folder
 */

#include "ShellBuffer.hh"
#include "ShellStream.hh"
#include "ShellTask.hh"

using namespace MultiShell;

Task::~Task() {
  // ...
}

bool Task::process_task(ShellStream& stream, int& afw) { // returns true on completion of task
  return true;
}

TaskList::~TaskList() {
  // ...
}

void TaskList::linked_item_push(LinkedItem& item) {
  Task *tptr = (Task *) &item;
  tptr->m_flags = 0;

  LinkedList::linked_item_push(item);
}

void TaskList::process_tasks(ShellStream& stream) {
  if (!stream) // no active serial connection
    return;

  Task *current = (Task *) linked_item(0);
  if (!m_flags && !current) // no tasks in queue
    return;

  int afw = stream.sync_write_begin(); // afw will be 0 or sufficient

  if (afw) {
    while ((m_flags || current) && afw) {
      if (m_flags & Task::fMaskRSVP) {
	stream.write(6, afw); // acknowledge
	m_flags &= ~Task::fMaskRSVP;
	continue;
      }

      unsigned char eol_count = m_flags & Task::fMaskEOL;
      if (eol_count) {
	stream.write_eol(afw);
	m_flags &= ~Task::fMaskEOL;
	m_flags |= --eol_count;
	continue;
      }

      if (!current->process_task(stream, afw))
	break;

      // task complete; return to owner
      m_flags = current->m_flags;
      pop_and_return(); // current is no longer valid
      break;
    }
    stream.sync_write_end();
  }
}

Task_OffsetString::~Task_OffsetString() {
  // ...
}

bool Task_OffsetString::process_task(ShellStream& stream, int& afw) { // returns true on completion of task
  if (!m_str) // oops
    return true;

  bool bDone = false;

  while (afw) {
    if (m_offset) {
      stream.write(' ', afw);
      --m_offset;
      continue;
    }
    if (*m_str) {
      stream.write(*m_str, afw);
      ++m_str;
      continue;
    }
    bDone = true;
    break;
  }
  return bDone;
}

Task_Buffer::~Task_Buffer() {
  // ...
}

bool Task_Buffer::process_task(ShellStream& stream, int& afw) { // returns true on completion of task
  if (m_bufptr == m_bufend)
    return true;

  bool bDone = false;

  while (afw) {
    if (m_bufptr < m_bufend) {
      stream.write(*m_bufptr, afw);
      ++m_bufptr;
      continue;
    }
    bDone = true;
    break;
  }
  return bDone;
}

int Task_Buffer::assign(const char *buffer, int length) {
  if (buffer && length > 0) {
    int maxlen = capacity();
    if (length > maxlen)
      length = maxlen;
    m_bufptr = m_bufend - length;
    memcpy(m_bufptr, buffer, length);
  } else {
    m_bufptr = m_bufend; // oops? clear
  }
  return count();
}

Task_Printable::~Task_Printable() {
  // ...
}

bool Task_Printable::process_task(ShellStream& stream, int& afw) { // returns true on completion of task
  static const char *error_message = "Command list error!";

  bool bDone = false;

  int pcount = m_list->printable_count();
  int icount = m_list->count();

  while (afw) {
    if (m_iindex < 0 && m_pindex < pcount) { // writing the list's description, etc., if any
      if (!m_ptr) {                          // no pointer set; get next printable
	if (++m_pindex >= pcount)            // the list has no (or no further) printables
	  continue;
	m_ptr = m_list->printable(m_pindex, m_offset);
	if (!m_ptr)
	  m_ptr = error_message;
	continue;
      }
      if (m_offset) {                        // print spaces if offset specified
	stream.write(' ', afw);
	--m_offset;
	continue;
      }
      if (!*m_ptr) {                         // reached the end of the current list printable
	stream.write_eol(afw);
	m_ptr = 0;
	continue;
      }
      stream.write(*m_ptr++, afw);
      continue;
    }
    
    if (!m_item) {                           // no pointer set; get next item
      if (++m_iindex >= icount) {            // the list has no (or no further) items
	bDone = true;
	break;
      }
      m_item = m_list->item(m_iindex);
      if (!m_item) { // oops
	bDone = true;
	break;
      }
      pcount = m_item->printable_count();
      m_pindex = -1;
      m_ptr = 0;
      continue;
    }

    if (!m_ptr) {                            // no pointer set; get next printable
      if (++m_pindex >= pcount) {            // the item has no (or no further) printables
	m_item = 0;
	continue;
      }
      m_ptr = m_item->printable(m_pindex, m_offset);
      if (!m_ptr)
	m_ptr = error_message;
      continue;
    }
    if (m_offset) {                          // print spaces if offset specified
      if (m_offset == 2 && m_iindex == m_list->selection())
	stream.write('*', afw);              // indicating the current selection, if any
      else
	stream.write(' ', afw);
      --m_offset;
      continue;
    }
    if (!*m_ptr) {                           // reached the end of the current item printable
      stream.write_eol(afw);
      m_ptr = 0;
      continue;
    }
    stream.write(*m_ptr++, afw);
    continue;
  }
  return bDone;
}

Task_Comma::~Task_Comma() {
  // ...
}

bool Task_Comma::process_task(ShellStream& stream, int& afw) { // returns true on completion of task
  bool bDone = false;

  while (afw) {
    if (m_command) {
      stream.write(m_command, afw);
      m_command = 0;
      continue;
    }
    if (m_digits) {
      unsigned long divisor = 1;
      for (int i = 1; i < m_digits; i++)
	divisor *= 10;
      unsigned long digit = m_value / divisor;
      m_value -= digit * divisor;
      --m_digits;
      stream.write('0' + digit, afw);
      continue;
    }
    stream.write(',', afw);
    bDone = true;
    break;
  }
  return bDone;
}

Repository::Repository() {
  for (int i = 0; i < 4; i++) {
    m_gpbuf[i].init(m_buf_gp[i], 128);
    m_owner_gp.push(m_gpbuf[i], true);
  }

  for (int i = 0; i < 16; i++) {
    m_owner_os.push(m_ostrs[i], true);
  }
  for (int i = 0; i < 2; i++) {
    m_owner_pl.push(m_plist[i], true);
  }
  for (int i = 0; i < 16; i++) {
    m_owner_cc.push(m_comma[i], true);
  }

  Task_Buffer *tptr = m_tasks;

  char *base = m_buf_16;
  for (int i = 0; i < 16; i++) {
    tptr->init(base, 16);
    base += 16;
    m_owner_16.push(*tptr++, true);
  }
  base = m_buf_32;
  for (int i = 0; i < 8; i++) {
    tptr->init(base, 32);
    base += 32;
    m_owner_32.push(*tptr++, true);
  }
  base = m_buf_64;
  for (int i = 0; i < 4; i++) {
    tptr->init(base, 64);
    base += 64;
    m_owner_64.push(*tptr++, true);
  }
}

static inline Task *s_buftask(TaskOwner<Task_Buffer>& owner, unsigned capacity, const char *& buffer, unsigned& length) {
  Task_Buffer *tptr = owner.pop();
  if (tptr) {
    int sublen = (length > capacity) ? capacity : length;
    tptr->assign(buffer, sublen);
    buffer += sublen;
    length -= sublen;
  }
  return tptr;
}

bool Repository::dispatch_buffer(TaskOwner<Task>& manager, const char *buffer, unsigned length) {
  while (length > 32) {
    Task *tptr = s_buftask(m_owner_64, 64, buffer, length);
    if (!tptr)
      break;
    manager.push(*tptr);
  }
  while (length > 16) {
    Task *tptr = s_buftask(m_owner_32, 32, buffer, length);
    if (!tptr)
      tptr = s_buftask(m_owner_64, 64, buffer, length);
    if (!tptr)
      break;
    manager.push(*tptr);
  }
  while (length) {
    Task *tptr = s_buftask(m_owner_16, 16, buffer, length);
    if (!tptr)
      tptr = s_buftask(m_owner_32, 32, buffer, length);
    if (!tptr)
      tptr = s_buftask(m_owner_64, 64, buffer, length);
    if (!tptr)
      break;
    manager.push(*tptr);
  }
  return (length == 0);
}

void Repository::status(ShellBuffer& buffer) {
  buffer.printf("Free currently, OffStr: %2d/16; List: %d/2; CC: %d/16; Buf-16: %2d/16; Buf-32: %d/8; Buf-64: %d/4",
		m_owner_os.count(), m_owner_pl.count(), m_owner_cc.count(),
		m_owner_16.count(), m_owner_32.count(), m_owner_64.count());
}

Dispatcher::~Dispatcher() {
  // ...
}
