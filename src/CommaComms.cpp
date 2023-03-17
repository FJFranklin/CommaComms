/* Copyright 2019-22 Francis James Franklin
 * 
 * Open Source under the MIT License - see LICENSE in the project's root folder
 */

#include "CommaComms.hh"

using namespace MultiShell;

// Pack754 Code originally by Brian "Beej Jorgensen" Hall in his Guide to Network Programming

uint32_t Comma::pack754_32(float f) {
  const unsigned bits = 32;
  const unsigned expbits = 8;
  const unsigned bias = (1 << (expbits - 1)) - 1;
  const unsigned significandbits = bits - expbits - 1; // -1 for sign bit
  const uint32_t signbit = 1UL << (bits - 1);

  if (f == 0.0) return 0; // get this special case out of the way

  // check sign and begin normalization
  uint32_t sign = (f < 0) ? signbit : 0;
  float fnorm = sign ? (-f) : f;

  // get the normalized form of f and track the exponent
  int shift = 0;
  while (fnorm >= 2.0) { fnorm /= 2.0; shift++; }
  while (fnorm <  1.0) { fnorm *= 2.0; shift--; }
  fnorm = fnorm - 1.0;

  // calculate the binary form (non-float) of the significand data
  uint32_t significand = fnorm * ((1L << significandbits) + 0.5f);

  // get the biased exponent
  uint32_t exp = shift + bias; // shift + bias

  // return the final answer
  return sign | (exp << significandbits) | significand;
}

float Comma::unpack754_32(uint32_t i) {
  const unsigned bits = 32;
  const unsigned expbits = 8;
  const uint32_t expmask = (1UL << expbits) - 1;
  const unsigned bias = (1 << (expbits - 1)) - 1;
  const unsigned significandbits = bits - expbits - 1; // -1 for sign bit
  const uint32_t significandmask = (1UL << significandbits) - 1;
  const uint32_t signbit = 1UL << (bits - 1);

  if (i == 0) return 0.0; // get this special case out of the way

  // extract and reconstruct the significand
  float result = i & significandmask;
  result /= (1UL << significandbits);
  result += 1.0f;

  // extract exponent and remove bias
  uint32_t exp = (i >> significandbits) & expmask;
  int shift = (int) exp - bias;

  while (shift > 0) { result *= 2.0; shift--; }
  while (shift < 0) { result /= 2.0; shift++; }

  return (i & signbit) ? (-result) : result;
}

bool Comma::push(char next, CommaCommand& command) {
  command.m_command = 0;
  command.m_value = 0;

  if ((next >= 'A' && next <= 'Z') || (next >= 'a' && next <= 'z')) {
    m_buffer[0] = next;
    m_length = 1;
  } else if (next >= '0' && next <= '9') {
    if (m_length > 0 && m_length < 11) {
      m_buffer[m_length++] = next;
    } else {
      m_length = 0;
    }
  } else if (m_length && next == ',') {
    command.m_command = m_buffer[0];
    if (m_length > 1) {
      m_buffer[m_length] = 0;
      if (sscanf(m_buffer + 1, "%lu", &command.m_value) != 1)
	command.m_command = 0;
      // command.m_value = strtoul(m_buffer+1, 0, 10);
    }
    m_length = 0;
  } else {
    m_length = 0;
  }
  return (command.m_command != 0);
}
