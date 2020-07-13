# Copyright 2020 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re
import socket
import struct
import subprocess
import time
import xml.etree.ElementTree

SOCKET_ADDR = ('localhost', 8765)

SIGTRAP = 5
SIGSEGV = 11
RETURNCODE_KILL = -9

ARCH = 'wasm32'
REG_DEFS = {
    ARCH: [('pc', 'Q'), ],
}


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

def RspChecksum(data):
  checksum = 0
  for char in data:
    checksum = (checksum + ord(char)) & 0xff
  return checksum

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

  def _GetReply(self):
    reply = ''
    message_finished = re.compile('#[0-9a-fA-F]{2}')
    while True:
      data = self._socket.recv(1024)
      if len(data) == 0:
        raise AssertionError('EOF on socket reached with '
                             'incomplete reply message: %r' % reply)
      reply += data
      if message_finished.match(reply[-3:]):
        break
    match = re.match('\+?\$([^#]*)#([0-9a-fA-F]{2})$', reply)
    if match is None:
      raise AssertionError('Unexpected reply message: %r' % reply)
    reply_body = match.group(1)
    checksum = match.group(2)
    expected_checksum = '%02x' % RspChecksum(reply_body)
    if checksum != expected_checksum:
      raise AssertionError('Bad RSP checksum: %r != %r' %
                           (checksum, expected_checksum))
    # Send acknowledgement.
    self._socket.send('+')
    return reply_body

  # Send an rsp message, but don't wait for or expect a reply.
  def RspSendOnly(self, data):
    msg = '$%s#%02x' % (data, RspChecksum(data))
    return self._socket.send(msg)

  def RspRequest(self, data):
    self.RspSendOnly(data)
    reply = self._GetReply()
    return reply

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


class LaunchDebugStub(object):
  def __init__(self, command):
    self._proc = PopenDebugStub(command)

  def __enter__(self):
    try:
      return GdbRspConnection()
    except:
      KillProcess(self._proc)
      raise

  def __exit__(self, exc_type, exc_value, traceback):
    KillProcess(self._proc)


def AssertEquals(x, y):
  if x != y:
    raise AssertionError('%r != %r' % (x, y))

def DecodeHex(data):
  assert len(data) % 2 == 0, data
  return bytes(bytearray([int(data[index * 2 : (index + 1) * 2], 16) for index in xrange(len(data) // 2)]))

def EncodeHex(data):
  return ''.join('%02x' % ord(byte) for byte in data)

def DecodeUInt64Array(data):
  assert len(data) % 16 == 0, data
  result = []
  for index in xrange(len(data) // 16):
    value = 0
    for digit in xrange(7, -1, -1):
      value = value * 256 + int(data[index * 16 + digit * 2 : index * 16 + (digit + 1) * 2], 16)
    result.append(value)
  return result

def AssertReplySignal(reply, signal):
  AssertEquals(ParseThreadStopReply(reply)['signal'], signal)

def ParseThreadStopReply(reply):
  match = re.match('T([0-9a-f]{2})thread-pcs:([0-9a-f]+);thread:([0-9a-f]+);$', reply)
  if not match:
    raise AssertionError('Bad thread stop reply: %r' % reply)
  return {'signal': int(match.group(1), 16),
          'thread_pc': int(match.group(2), 16),
          'thread_id': int(match.group(3), 16)}

def CheckInstructionPtr(connection, expected_ip):
  ip_value = DecodeRegs(connection.RspRequest('g'))['pc']
  AssertEquals(ip_value, expected_ip)

def DecodeRegs(reply):
  defs = REG_DEFS[ARCH]
  names = [reg_name for reg_name, reg_fmt in defs]
  fmt = ''.join([reg_fmt for reg_name, reg_fmt in defs])

  values = struct.unpack_from(fmt, DecodeHex(reply))
  return dict(zip(names, values))

def GetLoadedModules(connection):
  modules = {}
  reply = connection.RspRequest('qXfer:libraries:read')
  AssertEquals(reply[0], 'l')
  library_list = xml.etree.ElementTree.fromstring(reply[1:])
  AssertEquals(library_list.tag, 'library-list')
  for library in library_list:
    AssertEquals(library.tag, 'library')
    section = library.find('section')
    address = section.get('address')
    assert long(address) > 0
    modules[long(address)] = library.get('name')
  return modules

def GetLoadedModuleAddress(connection):
  modules = GetLoadedModules(connection)
  assert len(modules) > 0
  return modules.keys()[0]

def ReadCodeMemory(connection, address, size):
  reply = connection.RspRequest('m%x,%x' % (address, size))
  assert not reply.startswith('E'), reply
  return DecodeHex(reply)
