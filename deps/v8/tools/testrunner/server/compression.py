# Copyright 2012 the V8 project authors. All rights reserved.
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
#       copyright notice, this list of conditions and the following
#       disclaimer in the documentation and/or other materials provided
#       with the distribution.
#     * Neither the name of Google Inc. nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


import cStringIO as StringIO
try:
  import ujson as json
except ImportError:
  print("You should install UltraJSON, it is much faster!")
  import json
import os
import struct
import zlib

from . import constants

def Send(obj, sock):
  """
  Sends a JSON encodable object over the specified socket (zlib-compressed).
  """
  obj = json.dumps(obj)
  compression_level = 2  # 1 = fastest, 9 = best compression
  compressed = zlib.compress(obj, compression_level)
  payload = struct.pack('>i', len(compressed)) + compressed
  sock.sendall(payload)


class Receiver(object):
  def __init__(self, sock):
    self.sock = sock
    self.data = StringIO.StringIO()
    self.datalength = 0
    self._next = self._GetNext()

  def IsDone(self):
    return self._next == None

  def Current(self):
    return self._next

  def Advance(self):
    try:
      self._next = self._GetNext()
    except:
      raise

  def _GetNext(self):
    try:
      while self.datalength < constants.SIZE_T:
        try:
          chunk = self.sock.recv(8192)
        except:
          raise
        if not chunk: return None
        self._AppendData(chunk)
      size = self._PopData(constants.SIZE_T)
      size = struct.unpack(">i", size)[0]
      while self.datalength < size:
        try:
          chunk = self.sock.recv(8192)
        except:
          raise
        if not chunk: return None
        self._AppendData(chunk)
      result = self._PopData(size)
      result = zlib.decompress(result)
      result = json.loads(result)
      if result == constants.END_OF_STREAM:
        return None
      return result
    except:
      raise

  def _AppendData(self, new):
    self.data.seek(0, os.SEEK_END)
    self.data.write(new)
    self.datalength += len(new)

  def _PopData(self, length):
    self.data.seek(0)
    chunk = self.data.read(length)
    remaining = self.data.read()
    self.data.close()
    self.data = StringIO.StringIO()
    self.data.write(remaining)
    assert self.datalength - length == len(remaining)
    self.datalength = len(remaining)
    return chunk
