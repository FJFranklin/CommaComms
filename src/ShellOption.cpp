/* -*- mode: c++ -*-
 * 
 * Copyright 2020-22 Francis James Franklin
 * 
 * Open Source under the MIT License - see LICENSE in the project's root folder
 */

#include "ShellOption.hh"

using namespace MultiShell;

Option::~Option() {
  // ...
}

int Option::printable_count() const {
  return 1;
}

const char *Option::printable(int index, int& offset) const {
  offset = 3;
  return m_description;
}

OptionList::~OptionList() {
  // ...
}

int OptionList::printable_count() const {
  return 1;
}

const char *OptionList::printable(int index, int& offset) const {
  offset = 0;
  return m_description;
}

int OptionList::selection() const {
  return m_selection;
}
