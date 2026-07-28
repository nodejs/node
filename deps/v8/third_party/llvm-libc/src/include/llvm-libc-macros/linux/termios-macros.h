//===-- Definition of macros from termios.h -------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_MACROS_LINUX_TERMIOS_MACROS_H
#define LLVM_LIBC_MACROS_LINUX_TERMIOS_MACROS_H

// Below are generic definitions of symbolic bit-masks, modes etc. They serve
// most architectures including x86_64, aarch64 but have to be adjusted for few
// architectures MIPS.

#define NCCS 32

// Bit-masks for the c_iflag field of struct termios.
#define IGNBRK 0000001  // Ignore break condition
#define BRKINT 0000002  // Signal interrupt on break
#define IGNPAR 0000004  // Ignore characters with parity errors
#define PARMRK 0000010  // Mark parity and framing errors
#define INPCK 0000020   // Enable input parity check
#define ISTRIP 0000040  // Strip 8th bit off characters
#define INLCR 0000100   // Map NL to CR on input
#define IGNCR 0000200   // Ignore CR
#define ICRNL 0000400   // Map CR to NL on input
#define IUCLC 0001000   // Map uppercase characters to lowercase on input
#define IXON 0002000    // Enable start/stop output control
#define IXANY 0004000   // Enable any character to restart output
#define IXOFF 0010000   // Enable start/stop input control
#define IMAXBEL 0020000 // Ring bell when input queue is full
#define IUTF8 0040000   // Input is UTF8 (not in POSIX)

// Bit-masks for the c_oflag field of struct termios.
#define OPOST 0000001  // Post-process output
#define OLCUC 0000002  // Map lowercase characters to uppercase on output
#define ONLCR 0000004  // Map NL to CR-NL on output
#define OCRNL 0000010  // Map CR to NL on output
#define ONOCR 0000020  // No CR output at column 0
#define ONLRET 0000040 // NL performs CR function
#define OFILL 0000100  // Use fill characters for delay
#define OFDEL 0000200  // Fill is DEL
#define NLDLY 0000400  // Select newline delays
#define NL0 0000000    // Newline type 0
#define NL1 0000400    // Newline type 1
#define CRDLY 0003000  // Select carriage-return delays
#define CR0 0000000    // Carriage-return delay type 0
#define CR1 0001000    // Carriage-return delay type 1
#define CR2 0002000    // Carriage-return delay type 2
#define CR3 0003000    // Carriage-return delay type 3
#define TABDLY 0014000 // Select horizontal-tab delays
#define TAB0 0000000   // Horizontal-tab delay type 0
#define TAB1 0004000   // Horizontal-tab delay type 1
#define TAB2 0010000   // Horizontal-tab delay type 2
#define TAB3 0014000   // Expand tabs to spaces
#define BSDLY 0020000  // Select backspace delays
#define BS0 0000000    // Backspace-delay type 0
#define BS1 0020000    // Backspace-delay type 1
#define FFDLY 0100000  // Select form-feed delays
#define FF0 0000000    // Form-feed delay type 0
#define FF1 0100000    // Form-feed delay type 1
#define VTDLY 0040000  // Select vertical-tab delays
#define VT0 0000000    // Vertical-tab delay type 0
#define VT1 0040000    // Vertical-tab delay type 1
#define XTABS 0014000

// Symbolic subscripts for the c_cc array.
#define VINTR 0
#define VQUIT 1
#define VERASE 2
#define VKILL 3
#define VEOF 4
#define VTIME 5
#define VMIN 6
#define VSWTC 7
#define VSTART 8
#define VSTOP 9
#define VSUSP 10
#define VEOL 11
#define VREPRINT 12
#define VDISCARD 13
#define VWERASE 14
#define VLNEXT 15
#define VEOL2 16

// Baud rate related definitions
#define CBAUD 000000010017  // Baud speed mask
#define CBAUDX 000000010000 // Extra baud speed mask
#define CIBAUD 002003600000
#define CMSPAR 010000000000
#define CRTSCTS 020000000000
// Baud rates with values representable by the speed_t type.
#define B0 0000000 // Implies hang-up
// A symbol B<NN+> below indicates a baud rate of <NN+>.
#define B50 0000001
#define B75 0000002
#define B110 0000003
#define B134 0000004
#define B150 0000005
#define B200 0000006
#define B300 0000007
#define B600 0000010
#define B1200 0000011
#define B1800 0000012
#define B2400 0000013
#define B4800 0000014
#define B9600 0000015
#define B19200 0000016
#define B38400 0000017
// Extra baud rates
#define B57600 0010001
#define B115200 0010002
#define B230400 0010003
#define B460800 0010004
#define B500000 0010005
#define B576000 0010006
#define B921600 0010007
#define B1000000 0010010
#define B1152000 0010011
#define B1500000 0010012
#define B2000000 0010013
#define B2500000 0010014
#define B3000000 0010015
#define B3500000 0010016
#define B4000000 0010017

// Control mode bits for use in the c_cflag field of struct termios.
#define CSIZE 0000060 // Mask for character size bits
#define CS5 0000000
#define CS6 0000020
#define CS7 0000040
#define CS8 0000060
#define CSTOPB 0000100 // Send two bits, else one
#define CREAD 0000200  // Enable receiver
#define PARENB 0000400 // Parity enable
#define PARODD 0001000 // Odd parity, else even
#define HUPCL 0002000  // Hang up on last close
#define CLOCAL 0004000 // Ignore modem status lines

// Local mode bits for use in the c_lflag field of struct termios.
#define ISIG 0000001   // Enable signals
#define ICANON 0000002 // Canonical input (erase and kill processing)
#define ECHO 0000010   // Enable echo
#define ECHOE 0000020  // Echo erase character as error-correcting backspace
#define ECHOK 0000040  // Echo KILL
#define ECHONL 0000100 // Echo NL
#define NOFLSH 0000200 // Disable flush after interrupt or quit
#define TOSTOP 0000400 // Send SIGTTOU for background output

// Attribute selection
#define TCSANOW 0   // Change attributes immediately
#define TCSADRAIN 1 // Change attributes when output has drained
#define TCSAFLUSH 2 // Same as TCSADRAIN and flush pending Output

// Symbolic constants for use with tcflush function.
#define TCIFLUSH 0  // Flush pending input
#define TCIOFLUSH 1 // Flush pending input and unstransmitted output
#define TCOFLUSH 2  // Flush unstransmitted output

// Symbolic constantf for use with tcflow function.
#define TCOOFF 0 // Transmit a STOP character, intended to suspend input data
#define TCOON 1  // Transmit a START character, intended to restart input data
#define TCIOFF 2 // Suspend output
#define TCION 3  // Restart output

#endif // LLVM_LIBC_MACROS_LINUX_TERMIOS_MACROS_H
