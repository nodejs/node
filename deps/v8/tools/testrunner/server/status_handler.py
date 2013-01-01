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

from . import compression
from . import constants


def _StatusQuery(peer, query):
  sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
  code = sock.connect_ex((peer.address, constants.STATUS_PORT))
  if code != 0:
    # TODO(jkummerow): disconnect (after 3 failures?)
    return
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


def RequestTrustedPubkeys(peer, server):
  pubkey_list = _StatusQuery(peer, [constants.LIST_TRUSTED_PUBKEYS])
  for pubkey in pubkey_list:
    if server.IsTrusted(pubkey): continue
    result = _StatusQuery(peer, [constants.GET_SIGNED_PUBKEY, pubkey])
    server.AcceptNewTrusted(result)


def NotifyNewTrusted(peer, data):
  _StatusQuery(peer, [constants.NOTIFY_NEW_TRUSTED] + data)


def ITrustYouNow(peer):
  _StatusQuery(peer, [constants.TRUST_YOU_NOW])


def TryTransitiveTrust(peer, pubkey, server):
  if _StatusQuery(peer, [constants.DO_YOU_TRUST, pubkey]):
    result = _StatusQuery(peer, [constants.GET_SIGNED_PUBKEY, pubkey])
    server.AcceptNewTrusted(result)


class StatusHandler(SocketServer.BaseRequestHandler):
  def handle(self):
    rec = compression.Receiver(self.request)
    while not rec.IsDone():
      data = rec.Current()
      action = data[0]

      if action == constants.LIST_TRUSTED_PUBKEYS:
        response = self.server.daemon.ListTrusted()
        compression.Send([action, response], self.request)

      elif action == constants.GET_SIGNED_PUBKEY:
        response = self.server.daemon.SignTrusted(data[1])
        compression.Send([action, response], self.request)

      elif action == constants.NOTIFY_NEW_TRUSTED:
        self.server.daemon.AcceptNewTrusted(data[1:])
        pass  # No response.

      elif action == constants.TRUST_YOU_NOW:
        self.server.daemon.MarkPeerAsTrusting(self.client_address[0])
        pass  # No response.

      elif action == constants.DO_YOU_TRUST:
        response = self.server.daemon.IsTrusted(data[1])
        compression.Send([action, response], self.request)

      rec.Advance()
    compression.Send(constants.END_OF_STREAM, self.request)


class StatusSocketServer(SocketServer.ThreadingMixIn, SocketServer.TCPServer):
  def __init__(self, daemon):
    address = (daemon.ip, constants.STATUS_PORT)
    SocketServer.TCPServer.__init__(self, address, StatusHandler)
    self.daemon = daemon
