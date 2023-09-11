# Copyright 2018 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import random


def random_seed():
  """Returns random, non-zero seed."""
  seed = 0
  while not seed:
    seed = random.SystemRandom().randint(-2147483648, 2147483647)
  return seed
