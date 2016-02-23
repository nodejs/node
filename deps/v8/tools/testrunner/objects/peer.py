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


class Peer(object):
  def __init__(self, address, jobs, rel_perf, pubkey):
    self.address = address  # string: IP address
    self.jobs = jobs  # integer: number of CPUs
    self.relative_performance = rel_perf
    self.pubkey = pubkey # string: pubkey's fingerprint
    self.shells = set()  # set of strings
    self.needed_work = 0
    self.assigned_work = 0
    self.tests = []  # list of TestCase objects
    self.trusting_me = False  # This peer trusts my public key.
    self.trusted = False  # I trust this peer's public key.

  def __str__(self):
    return ("Peer at %s, jobs: %d, performance: %.2f, trust I/O: %s/%s" %
            (self.address, self.jobs, self.relative_performance,
             self.trusting_me, self.trusted))

  def AddTests(self, shell):
    """Adds tests from |shell| to this peer.

    Stops when self.needed_work reaches zero, or when all of shell's tests
    are assigned."""
    assert self.needed_work > 0
    if shell.shell not in self.shells:
      self.shells.add(shell.shell)
    while len(shell.tests) > 0 and self.needed_work > 0:
      t = shell.tests.pop()
      self.needed_work -= t.duration
      self.assigned_work += t.duration
      shell.total_duration -= t.duration
      self.tests.append(t)

  def ForceAddOneTest(self, test, shell):
    """Forcibly adds another test to this peer, disregarding needed_work."""
    if shell.shell not in self.shells:
      self.shells.add(shell.shell)
    self.needed_work -= test.duration
    self.assigned_work += test.duration
    shell.total_duration -= test.duration
    self.tests.append(test)


  def Pack(self):
    """Creates a JSON serializable representation of this Peer."""
    return [self.address, self.jobs, self.relative_performance]

  @staticmethod
  def Unpack(packed):
    """Creates a Peer object built from a packed representation."""
    pubkey_dummy = ""  # Callers of this don't care (only the server does).
    return Peer(packed[0], packed[1], packed[2], pubkey_dummy)
