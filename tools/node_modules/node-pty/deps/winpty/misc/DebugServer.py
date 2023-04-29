#!python
#
# Run with native CPython.  Needs pywin32 extensions.

# Copyright (c) 2011-2012 Ryan Prichard
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to
# deal in the Software without restriction, including without limitation the
# rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
# sell copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
# IN THE SOFTWARE.

import win32pipe
import win32api
import win32file
import time
import threading
import sys

# A message may not be larger than this size.
MSG_SIZE=4096

serverPipe = win32pipe.CreateNamedPipe(
    "\\\\.\\pipe\\DebugServer",
    win32pipe.PIPE_ACCESS_DUPLEX,
    win32pipe.PIPE_TYPE_MESSAGE | win32pipe.PIPE_READMODE_MESSAGE,
    win32pipe.PIPE_UNLIMITED_INSTANCES,
    MSG_SIZE,
    MSG_SIZE,
    10 * 1000,
    None)
while True:
    win32pipe.ConnectNamedPipe(serverPipe, None)
    (ret, data) = win32file.ReadFile(serverPipe, MSG_SIZE)
    print(data.decode())
    sys.stdout.flush()

    # The client uses CallNamedPipe to send its message.  CallNamedPipe waits
    # for a reply message.  If I send a reply, however, using WriteFile, then
    # sometimes WriteFile fails with:
    #     pywintypes.error: (232, 'WriteFile', 'The pipe is being closed.')
    # I can't figure out how to write a strictly correct pipe server, but if
    # I comment out the WriteFile line, then everything seems to work.  I
    # think the DisconnectNamedPipe call aborts the client's CallNamedPipe
    # call normally.

    try:
        win32file.WriteFile(serverPipe, b'OK')
    except:
        pass
    win32pipe.DisconnectNamedPipe(serverPipe)
