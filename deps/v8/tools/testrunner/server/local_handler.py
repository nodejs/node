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


import socket
import SocketServer
import StringIO

from . import compression
from . import constants


def LocalQuery(query):
  sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
  code = sock.connect_ex(("localhost", constants.CLIENT_PORT))
  if code != 0: return None
  compression.Send(query, sock)
  compression.Send(constants.END_OF_STREAM, sock)
  rec = compression.Receiver(sock)
  data = None
  while not rec.IsDone():
    data = rec.Current()
    assert data[0] == query[0]
    data = data[1]
    rec.Advance()
  sock.close()
  return data


class LocalHandler(SocketServer.BaseRequestHandler):
  def handle(self):
    rec = compression.Receiver(self.request)
    while not rec.IsDone():
      data = rec.Current()
      action = data[0]

      if action == constants.REQUEST_PEERS:
        with self.server.daemon.peer_list_lock:
          response = [ p.Pack() for p in self.server.daemon.peers
                       if p.trusting_me ]
        compression.Send([action, response], self.request)

      elif action == constants.UNRESPONSIVE_PEER:
        self.server.daemon.DeletePeer(data[1])

      elif action == constants.REQUEST_PUBKEY_FINGERPRINT:
        compression.Send([action, self.server.daemon.pubkey_fingerprint],
                         self.request)

      elif action == constants.REQUEST_STATUS:
        compression.Send([action, self._GetStatusMessage()], self.request)

      elif action == constants.ADD_TRUSTED:
        fingerprint = self.server.daemon.CopyToTrusted(data[1])
        compression.Send([action, fingerprint], self.request)

      elif action == constants.INFORM_DURATION:
        test_key = data[1]
        test_duration = data[2]
        arch = data[3]
        mode = data[4]
        self.server.daemon.AddPerfData(test_key, test_duration, arch, mode)

      elif action == constants.UPDATE_PERF:
        address = data[1]
        perf = data[2]
        self.server.daemon.UpdatePeerPerformance(data[1], data[2])

      rec.Advance()
    compression.Send(constants.END_OF_STREAM, self.request)

  def _GetStatusMessage(self):
    sio = StringIO.StringIO()
    sio.write("Peers:\n")
    with self.server.daemon.peer_list_lock:
      for p in self.server.daemon.peers:
        sio.write("%s\n" % p)
    sio.write("My own jobs: %d, relative performance: %.2f\n" %
              (self.server.daemon.jobs, self.server.daemon.relative_perf))
    # Low-priority TODO: Return more information. Ideas:
    #   - currently running anything,
    #   - time since last job,
    #   - time since last repository fetch
    #   - number of workpackets/testcases handled since startup
    #   - slowest test(s)
    result = sio.getvalue()
    sio.close()
    return result


class LocalSocketServer(SocketServer.ThreadingMixIn, SocketServer.TCPServer):
  def __init__(self, daemon):
    SocketServer.TCPServer.__init__(self, ("localhost", constants.CLIENT_PORT),
                                    LocalHandler)
    self.daemon = daemon
