# Copyright 2018 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from . import base


# Alphabet size determines the hashing radix. Choosing a prime number prevents
# clustering of the hashes.
HASHING_ALPHABET_SIZE = 2 ** 7 -1

def radix_hash(capacity, key):
  h = 0
  for character in key:
    h = (h * HASHING_ALPHABET_SIZE + ord(character)) % capacity

  return h


class ShardProc(base.TestProcFilter):
  """Processor distributing tests between shards.
  It hashes the unique test identifiers uses the hash to shard tests.
  """
  def __init__(self, myid, shards_count):
    """
    Args:
      myid: id of the shard within [0; shards_count - 1]
      shards_count: number of shards
    """
    super(ShardProc, self).__init__()

    assert myid >= 0 and myid < shards_count

    self._myid = myid
    self._shards_count = shards_count

  def _filter(self, test):
    return self._myid != radix_hash(self._shards_count, test.procid)
