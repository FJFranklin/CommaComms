/* -*- mode: c++ -*-
 * 
 * Copyright 2020-22 Francis James Franklin
 * 
 * Open Source under the MIT License - see LICENSE in the project's root folder
 */

#include "Shell.hh"
#include "ShellCommand.hh"

using namespace MultiShell;

void Args::first() {
  if (*m_arg == 0) // empty string
    return;

  char *ptr = m_arg;
  bool bContainsString = false;
  InputState is = is_Processing;

  while (true) {
    if (*ptr == 0) {
      m_swap = 0;
      break;
    }
    if ((*ptr == ' ') && (is == is_Processing)) {
      m_swap = ' ';
      *ptr = 0;
      break;
    }
    if (*ptr == '"') {
      bContainsString = true;

      if (is == is_Processing)
        is = is_String;
      else
        is = is_Processing;
    }
    ++ptr;
  }
  if (bContainsString) {
    char * one = ptr - 1;
    char * two = one;

    while (one >= m_arg) {
      if (*one == '"') {
        --one;
        continue;
      }
      if (one < two)
        *two = *one;
      --one;
      --two;
    }
    m_arg = two + 1;
  }
}

void Args::next() {
  while (*m_arg) // find end of current string
    ++m_arg;
  *m_arg = m_swap; // restore

  if (m_swap) {
    ++m_arg; // proceed to next
    first(); // process
  }
}

Command::~Command() {
  // ...
}

int Command::printable_count() const {
  return 2;
}

const char *Command::printable(int index, int& offset) const {
  offset = index ? 8 : 0;
  return index ? m_description : m_usage;
}

CommandList::~CommandList() {
  // ...
}

void ShellHandler::shell_notification(Shell& origin, const char *message) {
  // ...
}

void ShellHandler::comma_command(Shell& origin, CommaCommand& command) {
  // ...
}

CommandError ShellHandler::shell_command(Shell& origin, Args& args) {
  return ce_Okay;
}

CommandError CommandList::shell_command(Shell& origin, Args& args) {
  if (args == "RSVP") {
    origin.respond_to_RSVP();
    return ce_Okay;
  }
  if (args == "help") {
    origin << *this;
    return ce_Okay;
  }
  return ce_UnhandledCommand;
}
