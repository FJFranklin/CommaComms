/* -*- mode: c++ -*-
 * 
 * Copyright 2022 Francis James Franklin
 * 
 * Open Source under the MIT License - see LICENSE in the project's root folder
 */

#include <ShellUtils.hh>

using namespace MultiShell;

LinkedItem::~LinkedItem() {
  // ...
}

LinkedList::~LinkedList() {
  // ...
}

void LinkedList::linked_item_push(LinkedItem& item) {
  if (!m_next) {
    m_next = &item;
  } else {
    LinkedItem *iptr = m_next;
    while (iptr->m_next)
      iptr = iptr->m_next;
    iptr->m_next = &item;
  }
  item.m_next = 0;
  ++m_count;
}

LinkedItem *LinkedList::linked_item_pop() {
  LinkedItem *iptr = m_next;
  if (iptr) {
    m_next = iptr->m_next;
    iptr->m_next = 0;
    --m_count;
  }
  return iptr;
}

LinkedItem *LinkedList::linked_item(int index) const {
  LinkedItem *iptr = m_next;

  if (index >= 0 && index < m_count) {
    while (index--)
      iptr = iptr->m_next;
  }
  return iptr;
}

int PrintableItem::printable_count() const {
  return 0;
}

const char *PrintableItem::printable(int index, int& offset) const {
  offset = 0;
  return "(oops)";
}

PrintableItem::~PrintableItem() {
  // ...
}

int PrintableList::selection() const {
  return -1;
}

int PrintableList::printable_count() const {
  return 0;
}

const char *PrintableList::printable(int index, int& offset) const {
  offset = 0;
  return "(oops)";
}

PrintableList::~PrintableList() {
  // ...
}

Timer::~Timer() {
  // ...
}

void Timer::every_milli() { // runs once a millisecond, on average
  // ...
}

void Timer::every_10ms() { // runs once every 10ms, on average
  // ...
}

void Timer::every_tenth(int tenth) { // runs once every tenth of a second, where tenth = 0..9
  // ...
}

void Timer::every_second() { // runs once every second
  // ...
}

void Timer::tick() {
  // ...
}

void Timer::run() {
  int count_ms = 0;
  int count_10ms = 0;
  int count_tenths = 0;

  unsigned long previous_time = millis();

  m_stop = false;

  while (!m_stop) {
    tick();

    // our little internal real-time clock:
    unsigned long current_time = millis();

    if (current_time != previous_time) {
      ++previous_time;
      every_milli();

      if (++count_ms == 10) {
	count_ms = 0;
	every_10ms();

	if (++count_10ms == 10) {
	  count_10ms = 0;
	  every_tenth(count_tenths);

	  if (++count_tenths == 10) {
	    count_tenths = 0;
	    every_second();
	  }
	}
      }
    }
#ifdef OS_Linux
    usleep(1);
#endif
  }
}
