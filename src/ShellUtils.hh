/* -*- mode: c++ -*-
 * 
 * Copyright 2022 Francis James Franklin
 * 
 * Open Source under the MIT License - see LICENSE in the project's root folder
 */

#ifndef __ShellUtils_hh__
#define __ShellUtils_hh__

#ifndef OS_Linux
#include <Arduino.h>

#if defined(_SAMD21_) 
#include <avr/dtostrf.h>
#endif

/* Currently Bluetooth only works for the Feather M0
 */
#if defined(ADAFRUIT_FEATHER_M0)
#define FEATHER_M0_BTLE
#endif
#endif // ! OS_Linux

#ifdef OS_Linux
#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <unistd.h>
#endif

namespace MultiShell {

#ifdef OS_Linux
  extern unsigned long millis();
#endif

  enum CommandError
    {
     ce_Okay = 0,
     ce_IncorrectUsage,
     ce_UnhandledCommand,
     ce_OtherError
    };
  enum InputState
    {
     is_CC = 0, // CommaComms input processing
     is_Start,
     is_Discard,
     is_Processing,
     is_String
    };

  class LinkedItem;
  class LinkedList;

  class LinkedItemOwner {
    friend LinkedItem;
  protected:
    virtual void linked_item_push(LinkedItem& item) = 0;
    virtual LinkedItem *linked_item_pop() = 0;
  public:
    virtual ~LinkedItemOwner() { }
  };

  class LinkedItem {
    friend LinkedList;
  private:
    LinkedItem      *m_next;
    LinkedItemOwner *m_owner;
  public:
    LinkedItem() :
      m_next(0),
      m_owner(0)
    {
      // ...
    }
    virtual ~LinkedItem();

    inline void return_to_owner() {
      if (m_owner)
	m_owner->linked_item_push(*this);
    }
  };

  class LinkedList : public LinkedItemOwner {
  private:
    LinkedItem *m_next;
    int         m_count;
  public:
    LinkedList() : m_next(0), m_count(0) {
      // ...
    }
    virtual ~LinkedList();
  protected:
    inline void linked_item_adopt(LinkedItem& item) {
      item.m_owner = this;
    }
    virtual void linked_item_push(LinkedItem& item);
    virtual LinkedItem *linked_item_pop();
    LinkedItem *linked_item(int index) const;
  public:
    inline int count() const {
      return m_count;
    }
    inline void pop_and_return() {
      LinkedItem *iptr = linked_item_pop();
      if (iptr)
	iptr->return_to_owner();
    }
  };

  template<class T> class ItemOwner : public LinkedList {
  public:
    virtual ~ItemOwner() { }

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

  class PrintableItem : public LinkedItem {
  public:
    virtual ~PrintableItem();

    virtual int printable_count() const;
    virtual const char *printable(int index, int& offset) const;
  };

  class PrintableList : public ItemOwner<PrintableItem> {
  public:
    virtual ~PrintableList();

    virtual int selection() const;
    virtual int printable_count() const;
    virtual const char *printable(int index, int& offset) const;
  };

  class Timer {
  private:
    bool m_stop;

  public:
    Timer() :
      m_stop(false)
    {
      // ...
    }

    virtual ~Timer();

    virtual void every_milli();          // runs once a millisecond, on average
    virtual void every_10ms();           // runs once every 10ms, on average
    virtual void every_tenth(int tenth); // runs once every tenth of a second, where tenth = 0..9
    virtual void every_second();         // runs once every second
    virtual void tick();

    inline void stop() {
      m_stop = true;
    }
    void run();
  };

  /** FIFO is a byte buffer where bytes are added and removed in first-in first-out order.
   */
  class FIFO {
  private:
    char *buffer_start; ///< The buffer.
    char *buffer_end;   ///< Pointer to the end of the buffer.
    char *data_start;   ///< Pointer to the start of the data; if start == end, then no data.
    char *data_end;     ///< Pointer to the end of the data; must point to a writable byte.

  public:
    /** Empty the buffer for a fresh start.
     */
    inline void clear() {
      data_start = buffer_start;
      data_end   = buffer_start;
    }

    /** Returns true if the buffer is empty.
     */
    inline bool is_empty() const {
      return (data_start == data_end);
    }

    /** Add a byte to the buffer; returns true if there was space.
     */
    inline bool push(char byte) {
      bool bCanPush = true;

      if (data_start < data_end) {
	if ((data_start == buffer_start) && (data_end + 1 == buffer_end)) {
	  bCanPush = false;
	}
      } else if (data_start > data_end) {
	if (data_end + 1 == data_start) {
	  bCanPush = false;
	}
      } // else (data_start == data_end) // buffer must be empty

      if (bCanPush) {
	*data_end = byte;

	if (++data_end == buffer_end) {
	  data_end = buffer_start;
	}
      }
      return bCanPush;
    }

    /** Remove a byte from the buffer; returns true if the buffer wasn't empty.
     */
    inline bool pop(char& byte) {
      if (data_start == data_end) { // buffer must be empty
	return false;
      }
      byte = *data_start;

      if (++data_start == buffer_end) {
	data_start = buffer_start;
      }
      return true;
    }

    FIFO(char *buffer, unsigned length) :
      buffer_start(buffer),
      buffer_end(buffer+length),
      data_start(buffer),
      data_end(buffer)
    {
      // ...
    }

    ~FIFO() {
      // ...
    }

    /** Read (and remove) multiple bytes from the buffer.
     * \param ptr    Pointer to an external byte array where the data should be written.
     * \param length Number of bytes to read from the buffer, if possible.
     * \return The number of bytes actually read from the buffer.
     */
    int read(char *ptr, int length) {
      int count = 0;

      if (ptr && length) {
	if (data_end > data_start) {
	  count = data_end - data_start;             // i.e., bytes in FIFO
	  count = (count > length) ? length : count; // or length, if less

	  memcpy(ptr, data_start, count);
	  data_start += count;

	} else if (data_end < data_start) {
	  count = buffer_end - data_start;           // i.e., bytes in FIFO *at the end*
	  count = (count > length) ? length : count; // or length, if less

	  memcpy(ptr, data_start, count);
	  data_start += count;

	  if (data_start == buffer_end) { // wrap-around
	    data_start = buffer_start;

	    length -= count;                         // how much we still want to read

	    int extra = data_end - data_start;      // i.e., bytes in FIFO

	    if (length && extra) {                       // we can read more...
	      extra = (extra > length) ? length : extra; // or length, if less

	      memcpy(ptr, data_start, extra);
	      data_start += extra;

	      count += extra;
	    }
	  }
	} // else (data_end == data_start) => FIFO is empty
      }
      return count;
    }

    /** Write multiple bytes to the buffer.
     * \param ptr    Pointer to an external byte array where the data should be read from.
     * \param length Number of bytes to write to the buffer, if possible.
     * \return The number of bytes actually written to the buffer.
     */
    int write(const char *ptr, int length) {
      int count = 0;

      if (ptr && length) {
	if (data_end > data_start) {
	  /* this is where we need to worry about wrap-around
	   */
	  if (data_start == buffer_start) { // we're *not* able to wrap-around
	    count = buffer_end - data_end - 1;         // i.e., usable free space in FIFO *at the end*
	    count = (count > length) ? length : count; // or length, if less

	    memcpy(data_end, ptr, count);
	    data_end += count;

	  } else { // we *are* able to wrap-around
	    count = buffer_end - data_end;             // i.e., usable free space in FIFO *at the end*
	    count = (count > length) ? length : count; // or length, if less

	    memcpy(data_end, ptr, count);
	    data_end += count;

	    if (data_end == buffer_end) { // wrap-around
	      data_end = buffer_start;

	      length -= count;                             // how much we still want to write

	      int extra = data_start - data_end - 1;     // i.e., usable free space in FIFO

	      if (length && extra) {                       // we can write more...
		extra = (extra > length) ? length : extra; // or length, if less

		memcpy(data_end, ptr, extra);
		data_end += extra;

		count += extra;
	      }
	    }
	  }
	} else if (data_end < data_start) {
	  count = data_start - data_end - 1;         // i.e., usable free space in FIFO
	  count = (count > length) ? length : count; // or length, if less

	  /* don't need to worry about wrap-around
	   */
	  memcpy(data_end, ptr, count);
	  data_end += count;

	} else { // (data_end == data_start)
	  /* the FIFO is empty - we can move the pointers for convenience
	   */
	  data_start = buffer_start;
	  data_end   = buffer_start;

	  count = buffer_end - buffer_start - 1;     // i.e., maximum number of bytes the FIFO can hold
	  count = (count > length) ? length : count; // or length, if less

	  /* don't need to worry about wrap-around
	   */
	  memcpy(data_end, ptr, count);
	  data_end += count;
	}
      }
      return count;
    }

    int available() const {
      int count = 0;

      if (data_end > data_start) {
	count = data_end - data_start;         // i.e., bytes in FIFO
      } else if (data_end < data_start) {
	count  = buffer_end - data_start;      // i.e., bytes in FIFO *at the end*
	count += data_end - buffer_start;      // i.e., bytes in FIFO
      } // else (data_end == data_start) => FIFO is empty

      return count;
    }

    int availableForWrite() const {
      int count = 0;

      if (data_end > data_start) {
	/* this is where we need to worry about wrap-around
	 */
	if (data_start == buffer_start) { // we're *not* able to wrap-around
	  count = buffer_end - data_end - 1;       // i.e., usable free space in FIFO *at the end*
	} else { // we *are* able to wrap-around
	  count  = buffer_end - data_end;          // i.e., usable free space in FIFO *at the end*
	  count += data_start - buffer_start - 1;  // i.e., usable free space in FIFO
	}
      } else if (data_end < data_start) {
	count = data_start - data_end - 1;         // i.e., usable free space in FIFO
      } else { // (data_end == data_start)
	count = buffer_end - buffer_start - 1;     // i.e., maximum number of bytes the FIFO can hold
      }
      return count;
    }
  };

  class VirtualSerial {
  protected:
    char m_buffer_in[64];
    char m_buffer_out[64];

    FIFO m_in;
    FIFO m_out;

    bool m_bActive;

    virtual void sync_read() = 0;
    virtual void sync_write() = 0;
  public:
    VirtualSerial() :
      m_in(m_buffer_in, 64),
      m_out(m_buffer_out, 64),
      m_bActive(false)
    {
      // ...
    }

    virtual ~VirtualSerial() { }

    virtual bool begin(const char *& status, unsigned long baud) = 0;

    inline operator bool() {
      return m_bActive;
    }

    inline void update() {
      sync_read();
      sync_write();
    }

    inline int available() {
      return m_in.available();
    }
    inline int availableForWrite() {
      return m_out.availableForWrite();
    }

    inline int read() {
      char c;
      if (!m_in.pop(c))
	return -1;
      return c;
    }
    inline bool write(char c) {
      return m_out.push(c);
    }
  };

} // MultiShell

#endif /* !__ShellUtils_hh__ */
