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


class Shell(object):
  def __init__(self, shell):
    self.shell = shell
    self.tests = []
    self.total_duration = 0.0

  def AddSuite(self, suite):
    self.tests += suite.tests
    self.total_duration += suite.total_duration

  def SortTests(self):
    self.tests.sort(cmp=lambda x, y: cmp(x.duration, y.duration))


def Assign(suites, peers):
  total_work = 0.0
  for s in suites:
    total_work += s.CalculateTotalDuration()

  total_power = 0.0
  for p in peers:
    p.assigned_work = 0.0
    total_power += p.jobs * p.relative_performance
  for p in peers:
    p.needed_work = total_work * p.jobs * p.relative_performance / total_power

  shells = {}
  for s in suites:
    shell = s.shell()
    if not shell in shells:
      shells[shell] = Shell(shell)
    shells[shell].AddSuite(s)
  # Convert |shells| to list and sort it, shortest total_duration first.
  shells = [ shells[s] for s in shells ]
  shells.sort(cmp=lambda x, y: cmp(x.total_duration, y.total_duration))
  # Sort tests within each shell, longest duration last (so it's
  # pop()'ed first).
  for s in shells: s.SortTests()
  # Sort peers, least needed_work first.
  peers.sort(cmp=lambda x, y: cmp(x.needed_work, y.needed_work))
  index = 0
  for shell in shells:
    while len(shell.tests) > 0:
      while peers[index].needed_work <= 0:
        index += 1
        if index == len(peers):
          print("BIG FAT WARNING: Assigning tests to peers failed. "
                "Remaining tests: %d. Going to slow mode." % len(shell.tests))
          # Pick the least-busy peer. Sorting the list for each test
          # is terribly slow, but this is just an emergency fallback anyway.
          peers.sort(cmp=lambda x, y: cmp(x.needed_work, y.needed_work))
          peers[0].ForceAddOneTest(shell.tests.pop(), shell)
      # If the peer already has a shell assigned and would need this one
      # and then yet another, try to avoid it.
      peer = peers[index]
      if (shell.total_duration < peer.needed_work and
          len(peer.shells) > 0 and
          index < len(peers) - 1 and
          shell.total_duration <= peers[index + 1].needed_work):
        peers[index + 1].AddTests(shell)
      else:
        peer.AddTests(shell)
