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


import multiprocessing
import os
import shutil
import subprocess
import threading
import time

from . import daemon
from . import local_handler
from . import presence_handler
from . import signatures
from . import status_handler
from . import work_handler
from ..network import perfdata


class Server(daemon.Daemon):

  def __init__(self, pidfile, root, stdin="/dev/null",
               stdout="/dev/null", stderr="/dev/null"):
    super(Server, self).__init__(pidfile, stdin, stdout, stderr)
    self.root = root
    self.local_handler = None
    self.local_handler_thread = None
    self.work_handler = None
    self.work_handler_thread = None
    self.status_handler = None
    self.status_handler_thread = None
    self.presence_daemon = None
    self.presence_daemon_thread = None
    self.peers = []
    self.jobs = multiprocessing.cpu_count()
    self.peer_list_lock = threading.Lock()
    self.perf_data_lock = None
    self.presence_daemon_lock = None
    self.datadir = os.path.join(self.root, "data")
    pubkey_fingerprint_filename = os.path.join(self.datadir, "mypubkey")
    with open(pubkey_fingerprint_filename) as f:
      self.pubkey_fingerprint = f.read().strip()
    self.relative_perf_filename = os.path.join(self.datadir, "myperf")
    if os.path.exists(self.relative_perf_filename):
      with open(self.relative_perf_filename) as f:
        try:
          self.relative_perf = float(f.read())
        except:
          self.relative_perf = 1.0
    else:
      self.relative_perf = 1.0

  def run(self):
    os.nice(20)
    self.ip = presence_handler.GetOwnIP()
    self.perf_data_manager = perfdata.PerfDataManager(self.datadir)
    self.perf_data_lock = threading.Lock()

    self.local_handler = local_handler.LocalSocketServer(self)
    self.local_handler_thread = threading.Thread(
        target=self.local_handler.serve_forever)
    self.local_handler_thread.start()

    self.work_handler = work_handler.WorkSocketServer(self)
    self.work_handler_thread = threading.Thread(
        target=self.work_handler.serve_forever)
    self.work_handler_thread.start()

    self.status_handler = status_handler.StatusSocketServer(self)
    self.status_handler_thread = threading.Thread(
        target=self.status_handler.serve_forever)
    self.status_handler_thread.start()

    self.presence_daemon = presence_handler.PresenceDaemon(self)
    self.presence_daemon_thread = threading.Thread(
        target=self.presence_daemon.serve_forever)
    self.presence_daemon_thread.start()

    self.presence_daemon.FindPeers()
    time.sleep(0.5)  # Give those peers some time to reply.

    with self.peer_list_lock:
      for p in self.peers:
        if p.address == self.ip: continue
        status_handler.RequestTrustedPubkeys(p, self)

    while True:
      try:
        self.PeriodicTasks()
        time.sleep(60)
      except Exception, e:
        print("MAIN LOOP EXCEPTION: %s" % e)
        self.Shutdown()
        break
      except KeyboardInterrupt:
        self.Shutdown()
        break

  def Shutdown(self):
    with open(self.relative_perf_filename, "w") as f:
      f.write("%s" % self.relative_perf)
    self.presence_daemon.shutdown()
    self.presence_daemon.server_close()
    self.local_handler.shutdown()
    self.local_handler.server_close()
    self.work_handler.shutdown()
    self.work_handler.server_close()
    self.status_handler.shutdown()
    self.status_handler.server_close()

  def PeriodicTasks(self):
    # If we know peers we don't trust, see if someone else trusts them.
    with self.peer_list_lock:
      for p in self.peers:
        if p.trusted: continue
        if self.IsTrusted(p.pubkey):
          p.trusted = True
          status_handler.ITrustYouNow(p)
          continue
        for p2 in self.peers:
          if not p2.trusted: continue
          status_handler.TryTransitiveTrust(p2, p.pubkey, self)
    # TODO: Ping for more peers waiting to be discovered.
    # TODO: Update the checkout (if currently idle).

  def AddPeer(self, peer):
    with self.peer_list_lock:
      for p in self.peers:
        if p.address == peer.address:
          return
      self.peers.append(peer)
    if peer.trusted:
      status_handler.ITrustYouNow(peer)

  def DeletePeer(self, peer_address):
    with self.peer_list_lock:
      for i in xrange(len(self.peers)):
        if self.peers[i].address == peer_address:
          del self.peers[i]
          return

  def MarkPeerAsTrusting(self, peer_address):
    with self.peer_list_lock:
      for p in self.peers:
        if p.address == peer_address:
          p.trusting_me = True
          break

  def UpdatePeerPerformance(self, peer_address, performance):
    with self.peer_list_lock:
      for p in self.peers:
        if p.address == peer_address:
          p.relative_performance = performance

  def CopyToTrusted(self, pubkey_filename):
    with open(pubkey_filename, "r") as f:
      lines = f.readlines()
      fingerprint = lines[-1].strip()
    target_filename = self._PubkeyFilename(fingerprint)
    shutil.copy(pubkey_filename, target_filename)
    with self.peer_list_lock:
      for peer in self.peers:
        if peer.address == self.ip: continue
        if peer.pubkey == fingerprint:
          status_handler.ITrustYouNow(peer)
        else:
          result = self.SignTrusted(fingerprint)
          status_handler.NotifyNewTrusted(peer, result)
    return fingerprint

  def _PubkeyFilename(self, pubkey_fingerprint):
    return os.path.join(self.root, "trusted", "%s.pem" % pubkey_fingerprint)

  def IsTrusted(self, pubkey_fingerprint):
    return os.path.exists(self._PubkeyFilename(pubkey_fingerprint))

  def ListTrusted(self):
    path = os.path.join(self.root, "trusted")
    if not os.path.exists(path): return []
    return [ f[:-4] for f in os.listdir(path) if f.endswith(".pem") ]

  def SignTrusted(self, pubkey_fingerprint):
    if not self.IsTrusted(pubkey_fingerprint):
      return []
    filename = self._PubkeyFilename(pubkey_fingerprint)
    result = signatures.ReadFileAndSignature(filename)  # Format: [key, sig].
    return [pubkey_fingerprint, result[0], result[1], self.pubkey_fingerprint]

  def AcceptNewTrusted(self, data):
    # The format of |data| matches the return value of |SignTrusted()|.
    if not data: return
    fingerprint = data[0]
    pubkey = data[1]
    signature = data[2]
    signer = data[3]
    if not self.IsTrusted(signer):
      return
    if self.IsTrusted(fingerprint):
      return  # Already trusted.
    filename = self._PubkeyFilename(fingerprint)
    signer_pubkeyfile = self._PubkeyFilename(signer)
    if not signatures.VerifySignature(filename, pubkey, signature,
                                      signer_pubkeyfile):
      return
    return  # Nothing more to do.

  def AddPerfData(self, test_key, duration, arch, mode):
    data_store = self.perf_data_manager.GetStore(arch, mode)
    data_store.RawUpdatePerfData(str(test_key), duration)

  def CompareOwnPerf(self, test, arch, mode):
    data_store = self.perf_data_manager.GetStore(arch, mode)
    observed = data_store.FetchPerfData(test)
    if not observed: return
    own_perf_estimate = observed / test.duration
    with self.perf_data_lock:
      kLearnRateLimiter = 9999
      self.relative_perf *= kLearnRateLimiter
      self.relative_perf += own_perf_estimate
      self.relative_perf /= (kLearnRateLimiter + 1)
