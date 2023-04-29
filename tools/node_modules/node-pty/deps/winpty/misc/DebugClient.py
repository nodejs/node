#!python
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

import winerror
import win32pipe
import win32file
import win32api
import sys
import pywintypes
import time

if len(sys.argv) != 2:
    print("Usage: %s message" % sys.argv[0])
    sys.exit(1)

message = "[%05.3f %s]: %s" % (time.time() % 100000, sys.argv[0], sys.argv[1])

win32pipe.CallNamedPipe(
    "\\\\.\\pipe\\DebugServer",
    message.encode(),
    16,
    win32pipe.NMPWAIT_WAIT_FOREVER)
