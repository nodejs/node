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
import socket
import subprocess
import threading
import time

from . import distro
from ..local import execution
from ..local import perfdata
from ..objects import peer
from ..objects import workpacket
from ..server import compression
from ..server import constants
from ..server import local_handler
from ..server import signatures


def GetPeers():
  data = local_handler.LocalQuery([constants.REQUEST_PEERS])
  if not data: return []
  return [ peer.Peer.Unpack(p) for p in data ]


class NetworkedRunner(execution.Runner):
  def __init__(self, suites, progress_indicator, context, peers, workspace):
    self.suites = suites
    datapath = os.path.join("out", "testrunner_data")
    # TODO(machenbach): These fields should exist now in the superclass.
    # But there is no super constructor call. Check if this is a problem.
    self.perf_data_manager = perfdata.PerfDataManager(datapath)
    self.perfdata = self.perf_data_manager.GetStore(context.arch, context.mode)
    for s in suites:
      for t in s.tests:
        t.duration = self.perfdata.FetchPerfData(t) or 1.0
    self._CommonInit(suites, progress_indicator, context)
    self.tests = []  # Only used if we need to fall back to local execution.
    self.tests_lock = threading.Lock()
    self.peers = peers
    self.pubkey_fingerprint = None  # Fetched later.
    self.base_rev = subprocess.check_output(
        "cd %s; git log -1 --format=%%H --grep=git-svn-id" % workspace,
        shell=True).strip()
    self.base_svn_rev = subprocess.check_output(
        "cd %s; git log -1 %s"          # Get commit description.
        " | grep -e '^\s*git-svn-id:'"  # Extract "git-svn-id" line.
        " | awk '{print $2}'"           # Extract "repository@revision" part.
        " | sed -e 's/.*@//'" %         # Strip away "repository@".
        (workspace, self.base_rev), shell=True).strip()
    self.patch = subprocess.check_output(
        "cd %s; git diff %s" % (workspace, self.base_rev), shell=True)
    self.binaries = {}
    self.initialization_lock = threading.Lock()
    self.initialization_lock.acquire()  # Released when init is done.
    self._OpenLocalConnection()
    self.local_receiver_thread = threading.Thread(
        target=self._ListenLocalConnection)
    self.local_receiver_thread.daemon = True
    self.local_receiver_thread.start()
    self.initialization_lock.acquire()
    self.initialization_lock.release()

  def _OpenLocalConnection(self):
    self.local_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    code = self.local_socket.connect_ex(("localhost", constants.CLIENT_PORT))
    if code != 0:
      raise RuntimeError("Failed to connect to local server")
    compression.Send([constants.REQUEST_PUBKEY_FINGERPRINT], self.local_socket)

  def _ListenLocalConnection(self):
    release_lock_countdown = 1  # Pubkey.
    self.local_receiver = compression.Receiver(self.local_socket)
    while not self.local_receiver.IsDone():
      data = self.local_receiver.Current()
      if data[0] == constants.REQUEST_PUBKEY_FINGERPRINT:
        pubkey = data[1]
        if not pubkey: raise RuntimeError("Received empty public key")
        self.pubkey_fingerprint = pubkey
        release_lock_countdown -= 1
      if release_lock_countdown == 0:
        self.initialization_lock.release()
        release_lock_countdown -= 1  # Prevent repeated triggering.
      self.local_receiver.Advance()

  def Run(self, jobs):
    self.indicator.Starting()
    need_libv8 = False
    for s in self.suites:
      shell = s.shell()
      if shell not in self.binaries:
        path = os.path.join(self.context.shell_dir, shell)
        # Check if this is a shared library build.
        try:
          ldd = subprocess.check_output("ldd %s | grep libv8\\.so" % (path),
                                        shell=True)
          ldd = ldd.strip().split(" ")
          assert ldd[0] == "libv8.so"
          assert ldd[1] == "=>"
          need_libv8 = True
          binary_needs_libv8 = True
          libv8 = signatures.ReadFileAndSignature(ldd[2])
        except:
          binary_needs_libv8 = False
        binary = signatures.ReadFileAndSignature(path)
        if binary[0] is None:
          print("Error: Failed to create signature.")
          assert binary[1] != 0
          return binary[1]
        binary.append(binary_needs_libv8)
        self.binaries[shell] = binary
    if need_libv8:
      self.binaries["libv8.so"] = libv8
    distro.Assign(self.suites, self.peers)
    # Spawn one thread for each peer.
    threads = []
    for p in self.peers:
      thread = threading.Thread(target=self._TalkToPeer, args=[p])
      threads.append(thread)
      thread.start()
    try:
      for thread in threads:
        # Use a timeout so that signals (Ctrl+C) will be processed.
        thread.join(timeout=10000000)
      self._AnalyzePeerRuntimes()
    except KeyboardInterrupt:
      self.terminate = True
      raise
    except Exception, _e:
      # If there's an exception we schedule an interruption for any
      # remaining threads...
      self.terminate = True
      # ...and then reraise the exception to bail out.
      raise
    compression.Send(constants.END_OF_STREAM, self.local_socket)
    self.local_socket.close()
    if self.tests:
      self._RunInternal(jobs)
    self.indicator.Done()
    return not self.failed

  def _TalkToPeer(self, peer):
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(self.context.timeout + 10)
    code = sock.connect_ex((peer.address, constants.PEER_PORT))
    if code == 0:
      try:
        peer.runtime = None
        start_time = time.time()
        packet = workpacket.WorkPacket(peer=peer, context=self.context,
                                       base_revision=self.base_svn_rev,
                                       patch=self.patch,
                                       pubkey=self.pubkey_fingerprint)
        data, test_map = packet.Pack(self.binaries)
        compression.Send(data, sock)
        compression.Send(constants.END_OF_STREAM, sock)
        rec = compression.Receiver(sock)
        while not rec.IsDone() and not self.terminate:
          data_list = rec.Current()
          for data in data_list:
            test_id = data[0]
            if test_id < 0:
              # The peer is reporting an error.
              with self.lock:
                print("\nPeer %s reports error: %s" % (peer.address, data[1]))
              continue
            test = test_map.pop(test_id)
            test.MergeResult(data)
            try:
              self.perfdata.UpdatePerfData(test)
            except Exception, e:
              print("UpdatePerfData exception: %s" % e)
              pass  # Just keep working.
            with self.lock:
              perf_key = self.perfdata.GetKey(test)
              compression.Send(
                  [constants.INFORM_DURATION, perf_key, test.duration,
                   self.context.arch, self.context.mode],
                  self.local_socket)
              self.indicator.AboutToRun(test)
              has_unexpected_output = test.suite.HasUnexpectedOutput(test)
              if has_unexpected_output:
                self.failed.append(test)
                if test.output.HasCrashed():
                  self.crashed += 1
              else:
                self.succeeded += 1
              self.remaining -= 1
              self.indicator.HasRun(test, has_unexpected_output)
          rec.Advance()
        peer.runtime = time.time() - start_time
      except KeyboardInterrupt:
        sock.close()
        raise
      except Exception, e:
        print("Got exception: %s" % e)
        pass  # Fall back to local execution.
    else:
      compression.Send([constants.UNRESPONSIVE_PEER, peer.address],
                       self.local_socket)
    sock.close()
    if len(test_map) > 0:
      # Some tests have not received any results. Run them locally.
      print("\nNo results for %d tests, running them locally." % len(test_map))
      self._EnqueueLocally(test_map)

  def _EnqueueLocally(self, test_map):
    with self.tests_lock:
      for test in test_map:
        self.tests.append(test_map[test])

  def _AnalyzePeerRuntimes(self):
    total_runtime = 0.0
    total_work = 0.0
    for p in self.peers:
      if p.runtime is None:
        return
      total_runtime += p.runtime
      total_work += p.assigned_work
    for p in self.peers:
      p.assigned_work /= total_work
      p.runtime /= total_runtime
      perf_correction = p.assigned_work / p.runtime
      old_perf = p.relative_performance
      p.relative_performance = (old_perf + perf_correction) / 2.0
      compression.Send([constants.UPDATE_PERF, p.address,
                        p.relative_performance],
                       self.local_socket)
