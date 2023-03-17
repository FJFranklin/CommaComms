/* -*- mode: c++ -*-
 * 
 * Copyright 2022 Francis James Franklin
 * 
 * Open Source under the MIT License - see LICENSE in the project's root folder
 */

#include <Shell.hh>
#include <ShellExtra.hh>

#include <string>

using namespace MultiShell;

Command sc_plott("plot", "plot <option>", "Test plotting capability; <option> = [0],1,2,...");
Command sc_unimp("eh",   "eh",            "Unimplemented command");

class LocalShell : public Timer, public ShellStream::Responder, public ShellHandler {
private:
  CommandList  m_list;
  char         m_buftmp[128];
  ShellBuffer  m_B;
  Shell        m_one;
  ShellPlot    m_plot;
public:
  LocalShell(ShellStream& terminal) :
    m_list(this),
    m_B(m_buftmp, 128),
    m_one(terminal, m_list, '0')
  {
    m_list.add(sc_plott);
    m_list.add(sc_unimp);

    terminal.set_responder(this);
    m_one.set_handler(this);
  }

  virtual ~LocalShell() {
    // ...
  }

  virtual void every_milli() { // runs once a millisecond, on average
    // ...
  }

  virtual void every_10ms() { // runs once every 10ms, on average
    // ...
  }

  virtual void every_tenth(int tenth) { // runs once every tenth of a second, where tenth = 0..9
    // ...
  }

  virtual void every_second() { // runs once every second
    // ...
    // m_one << "Tick tock!" << 0;
  }

  virtual void tick() {
    m_one.update();
  }

  virtual void stream_notification(ShellStream& stream, const char *message) {
    if (strcmp(message, "end") == 0)
      stop();
    else
      fprintf(stderr, "\n=== Note [%s]: %s ===\n", stream.name(), message);
  }

  virtual void shell_notification(Shell& origin, const char *message) {
    fprintf(stderr, "\n=== Note {%s}: %s ===\n", origin.name(), message);
  }

  virtual void comma_command(Shell& origin, CommaCommand& command) {
    fprintf(stderr, "\n=== CC {%s}: %c %lu ===\n", origin.name(), command.m_command, command.m_value);
    // ...
  }

  virtual CommandError shell_command(Shell& origin, Args& args) {
    fprintf(stderr, "\n=== Shell {%s}: %s ===\n", origin.name(), args.c_str());

    CommandError ce = ce_Okay;

    if (args == "plot") {
      int option = 0;
      if (++args != "") {
	if (sscanf(args.c_str(), "%d", &option) != 1) {
	  option = 0;
	}
      }
      m_plot.demo(option, origin, this);
    } else {
      origin << "Oops! Command: \"" << args << "\"" << 0;
      while (++args != "") {
        origin << "          arg: \"" << args << "\"" << 0;
      }
      ce = ce_UnhandledCommand;
    }
    return ce;
  }
};

class Passthrough : public Timer, public ShellStream::Responder {
private:
  const char  *m_command;

  ShellStream *m_terminal;
  ShellStream *m_device;

  bool m_bExitWhenQuiet;
public:
  Passthrough(ShellStream& terminal, ShellStream& device, const char *command) :
    m_command(command),
    m_terminal(&terminal),
    m_device(&device),
    m_bExitWhenQuiet(false)
  {
    m_terminal->set_responder(this);
    m_device->set_responder(this);
  }
  ~Passthrough() {
    // ...
  }

  virtual void stream_notification(ShellStream& stream, const char *message) {
    if (strcmp(message, "end") == 0)
      stop();
    else if (strcmp(message, "RSVP") == 0)
      m_bExitWhenQuiet = true;
    else
      fprintf(stderr, "\n=== Note [%s]: %s ===\n", stream.name(), message);
  }
private:
  void cross_sync(ShellStream& from, ShellStream& to, bool bCommand = false) {
    int afr = 0;

    if (bCommand && m_command) {
      if (*m_command)
	afr = strlen(m_command);
      else
	m_command = 0;
    }
    if (!afr)
      afr = from.sync_read_begin();
    if (!afr) {
      if (!bCommand && m_bExitWhenQuiet)
	stop();
      return;
    }

    int afw = to.sync_write_begin();
    if (!afw) return;

    while (afr && afw) {
      char byte = 0;

      if (bCommand && m_command) {
	byte = *m_command++;
	--afr;
      } else {
	int value = from.read(afr);
	if (value < 0)
	  break;
	byte = value;
      }

      to.write(byte, afw);
    }
    to.sync_write_end();
  }
public:
  virtual void every_milli() { // runs once a millisecond, on average
    m_terminal->update();
    m_device->update();

    cross_sync(*m_device, *m_terminal);
    cross_sync(*m_terminal, *m_device, true);
  }

  virtual void every_10ms() { // runs once every 10ms, on average
    // ...
  }

  virtual void every_tenth(int tenth) { // runs once every tenth of a second, where tenth = 0..9
    // ...
  }

  virtual void every_second() { // runs once every second
    // ...
  }

  virtual void tick() {
    // ...
  }
};

void pass_through(const char *device_name, const char *command = 0) {
  Terminal terminal;
  ShellStream stream_terminal(terminal, 'T');

  GenericSerial device(device_name);
  ShellStream stream_device(device, 'D');

  const char *status = 0;

  if (!stream_terminal.begin(status)) {
    fprintf(stderr, "pass-through: error (terminal): %s\n", status);
  } else if (!stream_device.begin(status)) {
    fprintf(stderr, "pass-through: error (device: %s): %s\n", device_name, status);
  } else {
    Passthrough(stream_terminal, stream_device, command).run();
  }
}

void local_shell() {
  Terminal terminal;
  ShellStream stream_terminal(terminal, 'T');

  const char *status = 0;

  if (!stream_terminal.begin(status)) {
    fprintf(stderr, "local-shell: error (terminal): %s\n", status);
  } else {
    LocalShell(stream_terminal).run();
  }
}

int main(int argc, char **argv) {
  const char *device = "/dev/ttyACM0";

  std::string command = "";

  bool bLocal = false;

  for (int arg = 1; arg < argc; arg++) {
    if (strcmp(argv[arg], "--help") == 0) {
      fprintf(stderr, "\nmultishell [--help] [--device=usb|serial|arduino|/dev/<ID>]\n\n");
      fprintf(stderr, "  --help               Display this help.\n");
      fprintf(stderr, "  --command=<command>  Send command and exit.\n");
      fprintf(stderr, "  --device=<device>    where <device> is one of usb, serial, arduino.\n");
      fprintf(stderr, "                       /dev/<ID>. [Defaults to /dev/ttyACM0].\n");
      fprintf(stderr, "  --local              Local shell for testing.\n\n");
      return 0;
    }
    if (strcmp(argv[arg], "--local") == 0) {
      bLocal = true;
      break;
    }
    if (strncmp(argv[arg], "--command=", 10) == 0) {
      command += ";";
      command += argv[arg] + 10;
    }
    if (strncmp(argv[arg], "--device=", 9) == 0) {
      device = argv[arg] + 9;

      if (strcmp(device, "usb") == 0)
	device = "/dev/ttyUSB0"; // first serial-over-usb on linux
      else if (strcmp(device, "serial") == 0)
	device = "/dev/serial0"; // first serial-over-usb on pi
      else if (strcmp(device, "arduino") == 0)
	device = "/dev/ttyACM0"; // first serial-over-usb on pi
      else if (strncmp(device, "/dev/", 5) != 0) {
	fprintf (stderr, "multishell [--help] [--device=usb|serial|arduino|/dev/<ID>]\n");
	return -1;
      }
    }
  }

  if (bLocal) {
    local_shell();
  } else if (command != "") {
    command += ";RSVP,";
    pass_through(device, command.c_str());
  } else {
    pass_through(device);
  }
  return 0;
}
