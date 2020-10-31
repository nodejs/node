#!/usr/bin/python

# Copyright (C) 2018 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import os
import sys

import numpy as np
import scipy as sp
import seaborn as sns

from collections import defaultdict
from matplotlib import pyplot as plt


def main(argv):
  sns.set()

  # Map from key to map from iteration id to bytes allocated.
  distributions = defaultdict(lambda: defaultdict(int))
  ground_truth = {}
  for line in sys.stdin:
    stripped = line.strip()
    # Skip empty lines
    if not stripped:
      continue
    itr, code_location, size = stripped.split(" ")
    if itr == 'g':
      assert code_location not in ground_truth
      ground_truth[code_location] = int(size)
    else:
      assert int(itr) not in distributions[code_location]
      distributions[code_location][int(itr)] += int(size)

  # Map from key to list of bytes allocated, one for each iteration.
  flat_distributions = {
      key: value.values() for key, value in distributions.iteritems()
  }

  for key, value in flat_distributions.iteritems():
    print key, "ground truth %d " % ground_truth[key], sp.stats.describe(value)
    sns.distplot(value)
    plt.show()


if __name__ == '__main__':
  main(sys.argv)
