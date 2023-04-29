/*
 * Copyright (c) 2016 Ryan Prichard
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef WINPTY_CONSTANTS_H
#define WINPTY_CONSTANTS_H

/*
 * You may want to include winpty.h instead, which includes this header.
 *
 * This file is split out from winpty.h so that the agent can access the
 * winpty flags without also declaring the libwinpty APIs.
 */

/*****************************************************************************
 * Error codes. */

#define WINPTY_ERROR_SUCCESS                        0
#define WINPTY_ERROR_OUT_OF_MEMORY                  1
#define WINPTY_ERROR_SPAWN_CREATE_PROCESS_FAILED    2
#define WINPTY_ERROR_LOST_CONNECTION                3
#define WINPTY_ERROR_AGENT_EXE_MISSING              4
#define WINPTY_ERROR_UNSPECIFIED                    5
#define WINPTY_ERROR_AGENT_DIED                     6
#define WINPTY_ERROR_AGENT_TIMEOUT                  7
#define WINPTY_ERROR_AGENT_CREATION_FAILED          8



/*****************************************************************************
 * Configuration of a new agent. */

/* Create a new screen buffer (connected to the "conerr" terminal pipe) and
 * pass it to child processes as the STDERR handle.  This flag also prevents
 * the agent from reopening CONOUT$ when it polls -- regardless of whether the
 * active screen buffer changes, winpty continues to monitor the original
 * primary screen buffer. */
#define WINPTY_FLAG_CONERR              0x1ull

/* Don't output escape sequences. */
#define WINPTY_FLAG_PLAIN_OUTPUT        0x2ull

/* Do output color escape sequences.  These escapes are output by default, but
 * are suppressed with WINPTY_FLAG_PLAIN_OUTPUT.  Use this flag to reenable
 * them. */
#define WINPTY_FLAG_COLOR_ESCAPES       0x4ull

/* On XP and Vista, winpty needs to put the hidden console on a desktop in a
 * service window station so that its polling does not interfere with other
 * (visible) console windows.  To create this desktop, it must change the
 * process' window station (i.e. SetProcessWindowStation) for the duration of
 * the winpty_open call.  In theory, this change could interfere with the
 * winpty client (e.g. other threads, spawning children), so winpty by default
 * spawns a special agent process to create the hidden desktop.  Spawning
 * processes on Windows is slow, though, so if
 * WINPTY_FLAG_ALLOW_CURPROC_DESKTOP_CREATION is set, winpty changes this
 * process' window station instead.
 * See https://github.com/rprichard/winpty/issues/58. */
#define WINPTY_FLAG_ALLOW_CURPROC_DESKTOP_CREATION 0x8ull

#define WINPTY_FLAG_MASK (0ull \
    | WINPTY_FLAG_CONERR \
    | WINPTY_FLAG_PLAIN_OUTPUT \
    | WINPTY_FLAG_COLOR_ESCAPES \
    | WINPTY_FLAG_ALLOW_CURPROC_DESKTOP_CREATION \
)

/* QuickEdit mode is initially disabled, and the agent does not send mouse
 * mode sequences to the terminal.  If it receives mouse input, though, it
 * still writes MOUSE_EVENT_RECORD values into CONIN. */
#define WINPTY_MOUSE_MODE_NONE          0

/* QuickEdit mode is initially enabled.  As CONIN enters or leaves mouse
 * input mode (i.e. where ENABLE_MOUSE_INPUT is on and ENABLE_QUICK_EDIT_MODE
 * is off), the agent enables or disables mouse input on the terminal.
 *
 * This is the default mode. */
#define WINPTY_MOUSE_MODE_AUTO          1

/* QuickEdit mode is initially disabled, and the agent enables the terminal's
 * mouse input mode.  It does not disable terminal mouse mode (until exit). */
#define WINPTY_MOUSE_MODE_FORCE         2



/*****************************************************************************
 * winpty agent RPC call: process creation. */

/* If the spawn is marked "auto-shutdown", then the agent shuts down console
 * output once the process exits.  The agent stops polling for new console
 * output, and once all pending data has been written to the output pipe, the
 * agent closes the pipe.  (At that point, the pipe may still have data in it,
 * which the client may read.  Once all the data has been read, further reads
 * return EOF.) */
#define WINPTY_SPAWN_FLAG_AUTO_SHUTDOWN 1ull

/* After the agent shuts down output, and after all output has been written
 * into the pipe(s), exit the agent by closing the console.  If there any
 * surviving processes still attached to the console, they are killed.
 *
 * Note: With this flag, an RPC call (e.g. winpty_set_size) issued after the
 * agent exits will fail with an I/O or dead-agent error. */
#define WINPTY_SPAWN_FLAG_EXIT_AFTER_SHUTDOWN 2ull

/* All the spawn flags. */
#define WINPTY_SPAWN_FLAG_MASK (0ull \
    | WINPTY_SPAWN_FLAG_AUTO_SHUTDOWN \
    | WINPTY_SPAWN_FLAG_EXIT_AFTER_SHUTDOWN \
)



#endif /* WINPTY_CONSTANTS_H */
