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


from . import context
from . import testcase

class WorkPacket(object):
  def __init__(self, peer=None, context=None, tests=None, binaries=None,
               base_revision=None, patch=None, pubkey=None):
    self.peer = peer
    self.context = context
    self.tests = tests
    self.binaries = binaries
    self.base_revision = base_revision
    self.patch = patch
    self.pubkey_fingerprint = pubkey

  def Pack(self, binaries_dict):
    """
    Creates a JSON serializable object containing the data of this
    work packet.
    """
    need_libv8 = False
    binaries = []
    for shell in self.peer.shells:
      prefetched_binary = binaries_dict[shell]
      binaries.append({"name": shell,
                       "blob": prefetched_binary[0],
                       "sign": prefetched_binary[1]})
      if prefetched_binary[2]:
        need_libv8 = True
    if need_libv8:
      libv8 = binaries_dict["libv8.so"]
      binaries.append({"name": "libv8.so",
                       "blob": libv8[0],
                       "sign": libv8[1]})
    tests = []
    test_map = {}
    for t in self.peer.tests:
      test_map[t.id] = t
      tests.append(t.PackTask())
    result = {
      "binaries": binaries,
      "pubkey": self.pubkey_fingerprint,
      "context": self.context.Pack(),
      "base_revision": self.base_revision,
      "patch": self.patch,
      "tests": tests
    }
    return result, test_map

  @staticmethod
  def Unpack(packed):
    """
    Creates a WorkPacket object from the given packed representation.
    """
    binaries = packed["binaries"]
    pubkey_fingerprint = packed["pubkey"]
    ctx = context.Context.Unpack(packed["context"])
    base_revision = packed["base_revision"]
    patch = packed["patch"]
    tests = [ testcase.TestCase.UnpackTask(t) for t in packed["tests"] ]
    return WorkPacket(context=ctx, tests=tests, binaries=binaries,
                      base_revision=base_revision, patch=patch,
                      pubkey=pubkey_fingerprint)
