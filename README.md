# CommaComms

This is an ultrasimple language for portably and readably transmitting commands and information
typically over serial. There is no packet checking, and corrupt or unrecognised input is dropped.
If you are looking for secure packet transmission, there are good libraries out there to use
instead.

# MultiShell (Developed and tested for Teensy)

MultiShell processes input on one or more disconnectable Serial channels. Input strings are processed
into arg sequences. No dynamic memory allocation is used, and arguments are processed in-place.

See examples/Logger for example of usage.

Written by Francis James Franklin
MIT license, check license.txt for more information
All text above must be included in any redistribution
