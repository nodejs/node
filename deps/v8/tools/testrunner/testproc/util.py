#!/usr/bin/env python
# Copyright 2020 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import heapq
import random


class FixedSizeTopList():
  """Utility collection for gathering a fixed number of elements with the
  biggest value for the given key. It employs a heap from which we pop the
  smallest element when the collection is 'full'.

  If you need a reversed behaviour (collect min values) just provide an
  inverse key."""

  def __init__(self, size, key=None):
    self.size = size
    self.key = key or (lambda x: x)
    self.data = []
    self.discriminator = 0

  def add(self, elem):
    elem_k = self.key(elem)
    heapq.heappush(self.data, (elem_k, self.extra_key(), elem))
    if len(self.data) > self.size:
      heapq.heappop(self.data)

  def extra_key(self):
    # Avoid key clash in tuples sent to the heap.
    # We want to avoid comparisons on the last element of the tuple
    # since those elements might not be comparable.
    self.discriminator += 1
    return self.discriminator

  def as_list(self):
    original_data = [rec for (_, _, rec) in self.data]
    return sorted(original_data, key=self.key, reverse=True)
