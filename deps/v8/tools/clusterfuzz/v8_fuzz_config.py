# Copyright 2018 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import random

# List of configuration experiments for correctness fuzzing.
# List of <probability>, <1st config name>, <2nd config name>, <2nd d8>.
# Probabilities must add up to 100.
FOOZZIE_EXPERIMENTS = [
  [5, 'ignition', 'ignition_asm', 'd8'],
  [5, 'ignition', 'trusted', 'd8'],
  [5, 'ignition', 'trusted_opt', 'd8'],
  [10, 'ignition', 'slow_path', 'd8'],
  [5, 'ignition', 'slow_path_opt', 'd8'],
  [25, 'ignition', 'ignition_turbo', 'd8'],
  [20, 'ignition', 'ignition_turbo_opt', 'd8'],
  [5, 'ignition_turbo_opt', 'ignition_turbo_opt', 'clang_x86/d8'],
  [5, 'ignition_turbo', 'ignition_turbo', 'clang_x86/d8'],
  [5, 'ignition', 'ignition', 'clang_x86/d8'],
  [5, 'ignition', 'ignition', 'clang_x64_v8_arm64/d8'],
  [5, 'ignition', 'ignition', 'clang_x86_v8_arm/d8'],
]

class Config(object):
  def __init__(self, name, rng=None):
    self.name = name
    self.rng = rng or random.Random()

  def choose_foozzie_flags(self):
    """Randomly chooses a configuration from FOOZZIE_EXPERIMENTS.

    Returns: List of flags to pass to v8_foozzie.py fuzz harness.
    """
    acc = 0
    threshold = self.rng.random() * 100
    for prob, first_config, second_config, second_d8 in FOOZZIE_EXPERIMENTS:
      acc += prob
      if acc > threshold:
        return [
          '--first-config=' + first_config,
          '--second-config=' + second_config,
          '--second-d8=' + second_d8,
        ]
    assert False
