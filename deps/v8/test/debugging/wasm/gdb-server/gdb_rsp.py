# Copyright 2019 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import socket
import subprocess
import time

SOCKET_ADDR = ('localhost', 8765)


def EnsurePortIsAvailable(addr=SOCKET_ADDR):
  # As a sanity check, check that the TCP port is available by binding to it
  # ourselves (and then unbinding).  Otherwise, we could end up talking to an
  # old instance of the GDB stub that is still hanging around, or to some
  # unrelated service that uses the same port number. Of course, there is still
  # a race condition because an unrelated process could bind the port after we
  # unbind.
  sock = socket.socket()
  sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, True)
  sock.bind(addr)
  sock.close()


class GdbRspConnection(object):

  def __init__(self, addr=SOCKET_ADDR):
    self._socket = self._Connect(addr)

  def _Connect(self, addr):
    # We have to poll because we do not know when the GDB stub has successfully
    # done bind() on the TCP port. This is inherently unreliable.
    timeout_in_seconds = 10
    poll_time_in_seconds = 0.1
    for i in xrange(int(timeout_in_seconds / poll_time_in_seconds)):
      # On Mac OS X, we have to create a new socket FD for each retry.
      sock = socket.socket()
      # Do not delay sending small packets. This significantly speeds up debug
      # stub tests.
      sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, True)
      try:
        sock.connect(addr)
      except socket.error:
        # Retry after a delay.
        time.sleep(poll_time_in_seconds)
      else:
        return sock
    raise Exception('Could not connect to the debug stub in %i seconds'
                    % timeout_in_seconds)

  def Close(self):
    self._socket.close()


def PopenDebugStub(command):
  EnsurePortIsAvailable()
  return subprocess.Popen(command)


def KillProcess(process):
  if process.returncode is not None:
    # kill() won't work if we've already wait()'ed on the process.
    return
  try:
    process.kill()
  except OSError:
    if sys.platform == 'win32':
      # If process is already terminated, kill() throws
      # "WindowsError: [Error 5] Access is denied" on Windows.
      pass
    else:
      raise
  process.wait()
