/* -*- mode: c++ -*-
 * 
 * Copyright 2022 Francis James Franklin
 * 
 * Open Source under the MIT License - see LICENSE in the project's root folder
 */

#include "ShellBuffer.hh"

using namespace MultiShell;

char ShellBuffer::m_tmp[16] = { 0 }; // Common temporary buffer; not interrupt- or thread-safe
bool ShellBuffer::m_bInit = true;

void ShellBuffer::init() {
  /* On Feather M0 (but unnecessary for Teensy):
   * This is a curious hack. Forcing the linker to include dtostrf() has the side-effect
   * of fixing vsnprintf() to work properly with floats.
   */
  m_bInit = false;
#ifndef OS_Linux
  dtostrf(1.0f, 1, 2, m_tmp);
#endif
}
