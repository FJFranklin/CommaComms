#include "MultiShell.hh"

void ShellHandler::CommandArgs::first() {
  if (*m_arg == 0) // empty string
    return;

  char *ptr = m_arg;
  bool bContainsString = false;
  Shell::InputState is = Shell::is_Processing;

  while (true) {
    if (*ptr == 0) {
      m_swap = 0;
      break;
    }
    if ((*ptr == ' ') && (is == Shell::is_Processing)) {
      m_swap = ' ';
      *ptr = 0;
      break;
    }
    if (*ptr == '"') {
      bContainsString = true;

      if (is == Shell::is_Processing)
        is = Shell::is_String;
      else
        is = Shell::is_Processing;
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

void ShellHandler::CommandArgs::next() {
  while (*m_arg) // find end of current string
    ++m_arg;
  *m_arg = m_swap; // restore

  if (m_swap) {
    ++m_arg; // proceed to next
    first(); // process
  }
}

void Shell::tick() {
  if (!active()) {
    m_state = is_Start;
    m_index = 0;
    return;
  }

  char *ptr = m_buffer + m_index;

  while (true) {
    int ic = read();
    if (ic < 0) break;

    char c = ic;

    bool bEOL = ((c == '\n') || (c == '\r'));
    bool bWS = isspace(c);
    bool bGR = isgraph(c);

    if (m_state == is_Discard) {
      if (bEOL)
        m_state = is_Start;
      continue;
    }
    if (!bWS && !bGR) { // character outside useful range
      write("Error! Unexpected character");
      m_state = is_Discard; // reset
      m_index = 0;
      ptr = m_buffer;
      continue;
    }

    if (bEOL) {
      if (m_state == is_String) {
        write("Error! EOL within string\n");
      } else if (m_state != is_Start) {
        if (m_index)
          if (*(ptr - 1) == ' ') { // The last character was a space
            --ptr;
            --m_index;
          }
        *ptr = 0;

        ShellHandler::CommandArgs args(m_buffer);
        const ShellCommand * sc = m_command_list->lookup(args.c_str());
        if (!sc) {
          write("Error! Command \"");
          write(args.c_str());
          write("\" not recognised. Try 'help'.\n");
        } else {
          ShellHandler *handler = sc->m_handler;

          if (!handler)
            handler = m_command_list->default_handler();
          if (!handler)
            write("Error! (Internal: No default handler set).\n");
          else {
            ShellHandler::CommandError ce = handler->shell_command(*this, args);

            if (ce == ShellHandler::ce_IncorrectUsage) {
              write("Error! Incorrect usage. Try 'help'.\n");
            }
            if (ce == ShellHandler::ce_UnhandledCommand) {
              write("Error! (Internal: No handler).\n");
            }
          }
        }
      }
      m_state = is_Start;
      m_index = 0;
      ptr = m_buffer;
      continue;
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

    if (m_index == MultiShell_BufferSize - 1) {
      write("Error! Command too long.");
      m_state = is_Discard; // reset
      m_index = 0;
      ptr = m_buffer;
      continue;
    }

    // write(&c, 1);
    *ptr++ = c;
    ++m_index;

    if (m_state == is_Start)
      m_state = is_Processing;
  }
}
