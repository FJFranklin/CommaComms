/* -*- mode: c++ -*-
 * 
 * Copyright 2022 Francis James Franklin
 * 
 * Open Source under the MIT License - see LICENSE in the project's root folder
 */

#include "Shell.hh"

#include <ctype.h>

using namespace MultiShell;

Repository Shell::m_repository;

Shell::~Shell() {
  // ...
}

void Shell::update() {
  m_stream->update(); // housekeeping for in & out

  if (!*m_stream) { // no active serial connection
    reset();
    return;
  }

  /* data-out processing
   */
  m_manager.process_tasks(*m_stream);

  /* the rest is data-in processing
   */
  int count = m_stream->sync_read_begin();

  if (m_state == is_CC) { // CommaComms input mode
    CommaCommand C;
    while (count) {
      int ic = m_stream->read(count);
      if (ic < 0)
	break;

      char c = ic;

      if (c == ';') {
	if (m_comma.push(',', C))
	  if (m_handler)
	    m_handler->comma_command(*this, C);
	reset(is_Start);
	break;
      }
      if (m_comma.push(c, C))
	if (m_handler)
	  m_handler->comma_command(*this, C);
    }
    if (m_state == is_CC) { // (still) CommaComms input mode
      return;
    }
  }

  char *ptr = m_buffer + m_index;

  while (count) {
    int ic = m_stream->read(count);
    if (ic < 0)
      break;

    char c = ic;
    if (c == 4) {
      if (m_handler)
	m_handler->shell_notification(*this, "end");
      break;
    }

    bool bEOL = (c == '\n') || (c == '\r') || ((m_state != is_String) && ((c == ',') || (c == ';')));
    bool bWS = isspace(c);
    bool bGR = isgraph(c);

    if (m_state == is_Discard) {
      if (bEOL) {
        reset((c == ';') ? is_Start : is_CC);
	break;
      }
      continue;
    }
    if (!bWS && !bGR) { // character outside useful range
      *this << "Error! Unexpected character" << 0;
      m_state = is_Discard; // reset
      m_index = 0;
      ptr = m_buffer;
      continue;
    }

    if (bEOL) {
      if (m_state == is_String) {
        *this << "Error! EOL within string" << 0;
      } else if (m_state != is_Start) {
        if (m_index)
          if (*(ptr - 1) == ' ') { // The last character was a space
            --ptr;
            --m_index;
          }
        *ptr = 0;

        Args args(m_buffer);
        const Command * sc = m_command_list->lookup(args.c_str());
        if (!sc) {
          *this << "Error! Command \"" << args << "\" not recognised. Try 'help'." << 0;
        } else {
          ShellHandler *handler = sc->handler();

          if (!handler)
            handler = m_command_list->default_handler();
          if (!handler)
            *this << "Error! (Internal: No default handler set)." << 0;
          else {
            CommandError ce = handler->shell_command(*this, args);

            if (ce == ce_IncorrectUsage) {
              *this << "Error! Incorrect usage. Try 'help'." << 0;
            }
            if (ce == ce_UnhandledCommand) {
              *this << "Error! (Internal: No handler)." << 0;
            }
          }
        }
      }
      reset((c == ';') ? is_Start : is_CC);
      break;
    }
    if (bWS) {
      if (m_state == is_Start)
        continue;
      if (m_state != is_String) {
        if (m_index)
          if (*(ptr - 1) == ' ') // The last character was also a space
            continue;
        c = ' ';
      }
    }
    if (c == '"') {
      if (m_state == is_String) {
        if (m_index)
          if (*(ptr - 1) == '"') { // Hmm, empty string
            --ptr;
            --m_index;
            m_state = m_index ? is_Processing : is_Start;
            continue;
          }
        m_state = m_index ? is_Processing : is_Start;
      } else {
        m_state = is_String;
      }
    }

    if (m_index == BufferSize - 1) {
      *this << "Error! Command too long." << 0;
      m_state = is_Discard; // reset
      m_index = 0;
      ptr = m_buffer;
      continue;
    }

    *ptr++ = c;
    ++m_index;

    if (m_state == is_Start)
      m_state = is_Processing;
  }
}
