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
import threading
try:
  import ujson as json
except:
  import json

from . import constants
from ..objects import peer


STARTUP_REQUEST = "V8 test peer starting up"
STARTUP_RESPONSE = "Let's rock some tests!"
EXIT_REQUEST = "V8 testing peer going down"


def GetOwnIP():
  s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
  s.connect(("8.8.8.8", 80))
  ip = s.getsockname()[0]
  s.close()
  return ip


class PresenceHandler(SocketServer.BaseRequestHandler):

  def handle(self):
    data = json.loads(self.request[0].strip())

    if data[0] == STARTUP_REQUEST:
      jobs = data[1]
      relative_perf = data[2]
      pubkey_fingerprint = data[3]
      trusted = self.server.daemon.IsTrusted(pubkey_fingerprint)
      response = [STARTUP_RESPONSE, self.server.daemon.jobs,
                  self.server.daemon.relative_perf,
                  self.server.daemon.pubkey_fingerprint, trusted]
      response = json.dumps(response)
      self.server.SendTo(self.client_address[0], response)
      p = peer.Peer(self.client_address[0], jobs, relative_perf,
                    pubkey_fingerprint)
      p.trusted = trusted
      self.server.daemon.AddPeer(p)

    elif data[0] == STARTUP_RESPONSE:
      jobs = data[1]
      perf = data[2]
      pubkey_fingerprint = data[3]
      p = peer.Peer(self.client_address[0], jobs, perf, pubkey_fingerprint)
      p.trusted = self.server.daemon.IsTrusted(pubkey_fingerprint)
      p.trusting_me = data[4]
      self.server.daemon.AddPeer(p)

    elif data[0] == EXIT_REQUEST:
      self.server.daemon.DeletePeer(self.client_address[0])
      if self.client_address[0] == self.server.daemon.ip:
        self.server.shutdown_lock.release()


class PresenceDaemon(SocketServer.ThreadingMixIn, SocketServer.UDPServer):
  def __init__(self, daemon):
    self.daemon = daemon
    address = (daemon.ip, constants.PRESENCE_PORT)
    SocketServer.UDPServer.__init__(self, address, PresenceHandler)
    self.shutdown_lock = threading.Lock()

  def shutdown(self):
    self.shutdown_lock.acquire()
    self.SendToAll(json.dumps([EXIT_REQUEST]))
    self.shutdown_lock.acquire()
    self.shutdown_lock.release()
    SocketServer.UDPServer.shutdown(self)

  def SendTo(self, target, message):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.sendto(message, (target, constants.PRESENCE_PORT))
    sock.close()

  def SendToAll(self, message):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    ip = self.daemon.ip.split(".")
    for i in range(1, 254):
      ip[-1] = str(i)
      sock.sendto(message, (".".join(ip), constants.PRESENCE_PORT))
    sock.close()

  def FindPeers(self):
    request = [STARTUP_REQUEST, self.daemon.jobs, self.daemon.relative_perf,
               self.daemon.pubkey_fingerprint]
    request = json.dumps(request)
    self.SendToAll(request)
