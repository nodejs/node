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


import os
import SocketServer
import stat
import subprocess
import threading

from . import compression
from . import constants
from . import signatures
from ..network import endpoint
from ..objects import workpacket


class WorkHandler(SocketServer.BaseRequestHandler):

  def handle(self):
    rec = compression.Receiver(self.request)
    while not rec.IsDone():
      data = rec.Current()
      with self.server.job_lock:
        self._WorkOnWorkPacket(data)
      rec.Advance()

  def _WorkOnWorkPacket(self, data):
    server_root = self.server.daemon.root
    v8_root = os.path.join(server_root, "v8")
    os.chdir(v8_root)
    packet = workpacket.WorkPacket.Unpack(data)
    self.ctx = packet.context
    self.ctx.shell_dir = os.path.join("out",
                                      "%s.%s" % (self.ctx.arch, self.ctx.mode))
    if not os.path.isdir(self.ctx.shell_dir):
      os.makedirs(self.ctx.shell_dir)
    for binary in packet.binaries:
      if not self._UnpackBinary(binary, packet.pubkey_fingerprint):
        return

    if not self._CheckoutRevision(packet.base_revision):
      return

    if not self._ApplyPatch(packet.patch):
      return

    tests = packet.tests
    endpoint.Execute(v8_root, self.ctx, tests, self.request, self.server.daemon)
    self._SendResponse()

  def _SendResponse(self, error_message=None):
    try:
      if error_message:
        compression.Send([[-1, error_message]], self.request)
      compression.Send(constants.END_OF_STREAM, self.request)
      return
    except Exception, e:
      pass  # Peer is gone. There's nothing we can do.
    # Clean up.
    self._Call("git checkout -f")
    self._Call("git clean -f -d")
    self._Call("rm -rf %s" % self.ctx.shell_dir)

  def _UnpackBinary(self, binary, pubkey_fingerprint):
    binary_name = binary["name"]
    if binary_name == "libv8.so":
      libdir = os.path.join(self.ctx.shell_dir, "lib.target")
      if not os.path.exists(libdir): os.makedirs(libdir)
      target = os.path.join(libdir, binary_name)
    else:
      target = os.path.join(self.ctx.shell_dir, binary_name)
    pubkeyfile = "../trusted/%s.pem" % pubkey_fingerprint
    if not signatures.VerifySignature(target, binary["blob"],
                                      binary["sign"], pubkeyfile):
      self._SendResponse("Signature verification failed")
      return False
    os.chmod(target, stat.S_IRWXU)
    return True

  def _CheckoutRevision(self, base_svn_revision):
    get_hash_cmd = (
        "git log -1 --format=%%H --remotes --grep='^git-svn-id:.*@%s'" %
        base_svn_revision)
    try:
      base_revision = subprocess.check_output(get_hash_cmd, shell=True)
      if not base_revision: raise ValueError
    except:
      self._Call("git fetch")
      try:
        base_revision = subprocess.check_output(get_hash_cmd, shell=True)
        if not base_revision: raise ValueError
      except:
        self._SendResponse("Base revision not found.")
        return False
    code = self._Call("git checkout -f %s" % base_revision)
    if code != 0:
      self._SendResponse("Error trying to check out base revision.")
      return False
    code = self._Call("git clean -f -d")
    if code != 0:
      self._SendResponse("Failed to reset checkout")
      return False
    return True

  def _ApplyPatch(self, patch):
    if not patch: return True  # Just skip if the patch is empty.
    patchfilename = "_dtest_incoming_patch.patch"
    with open(patchfilename, "w") as f:
      f.write(patch)
    code = self._Call("git apply %s" % patchfilename)
    if code != 0:
      self._SendResponse("Error applying patch.")
      return False
    return True

  def _Call(self, cmd):
    return subprocess.call(cmd, shell=True)


class WorkSocketServer(SocketServer.ThreadingMixIn, SocketServer.TCPServer):
  def __init__(self, daemon):
    address = (daemon.ip, constants.PEER_PORT)
    SocketServer.TCPServer.__init__(self, address, WorkHandler)
    self.job_lock = threading.Lock()
    self.daemon = daemon
