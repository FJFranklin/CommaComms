/* -*- mode: c++ -*-
 * 
 * Copyright 2020-21 Francis James Franklin
 * 
 * Open Source under the MIT License - see LICENSE in the project's root folder
 */

#include "config.hh"
#include "Timer.hh"
#include "CC_Serial.hh"
#include "BTCommander.hh"

class BTComms : public Timer, public CommaComms::CC_Responder {
private:
  CC_Serial s0;
  CC_Serial s1;

  BTCommander *bt;

public:
  BTComms() :
    s0(Serial, '0', this),
    s1(Serial1, '1', this),

    bt(BTCommander::commander(this)) // zero if no Bluetooth connection is possible
  {
    // ...
  }
  virtual ~BTComms() {
    // ...
  }

  virtual void notify(CommaComms * C, const char * str) {
    if (Serial) {
      Serial.print(C->name());
      Serial.print(": notify: ");
      Serial.println(str);
    }

    if (strcmp(str, "bluetooth: disconnect") == 0) { // oops
      if (Serial) {
        Serial.println("Emergency stop!");
      }
      s1.command_send('x');
    }
  }
  virtual void command(CommaComms * C, char code, unsigned long value) {
#ifdef ENABLE_FEEDBACK
    if (Serial) {
      Serial.print(C->name());
      Serial.print(": command: ");
      Serial.print(code);
      Serial.print(": ");
      Serial.println(value);
    }
#endif

    if (code == 'p') {
      s0.ui((char) (value & 0xFF));
      if (bt) {
        bt->ui((char) (value & 0xFF));
      }      
    } else if (strcmp(C->name(), "s1") == 0) { // command received from Serial1; forward to others
      s0.command_send(code, value);
      if (bt) {
        bt->command_send(code, value);
      }
    } else {                                   // command received from others; forward to Serial1
      s1.command_send(code, value);
    }
  }

  virtual void every_milli() { // runs once a millisecond, on average
    // ...
  }

  virtual void every_10ms() { // runs once every 10ms, on average
    // ...
  }

  virtual void every_tenth(int tenth) { // runs once every tenth of a second, where tenth = 0..9
    digitalWrite(LED_BUILTIN, tenth == 0 || tenth == 8); // double blink per second

    if (bt) { // important: housekeeping
      bt->update();
    }
  }

  virtual void every_second() { // runs once every second
    // ...
  }

  virtual void tick() {
    s0.update(); // important: housekeeping
    s1.update(); // important: housekeeping
  }
};

void setup() {
  while (millis() < 500) {
    if (Serial) break;
  }
  Serial.begin(115200);
  while (millis() < 500) {
    if (Serial1) break;
  }
  Serial1.begin(115200);

  pinMode(LED_BUILTIN, OUTPUT);

  BTComms().run();
}

void loop() {
  // ...
}
