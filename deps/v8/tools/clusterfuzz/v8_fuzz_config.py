# Copyright 2018 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import random

# List of configuration experiments for correctness fuzzing.
# List of <probability>, <1st config name>, <2nd config name>, <2nd d8>.
# Probabilities must add up to 100.
FOOZZIE_EXPERIMENTS = [
  [10, 'ignition', 'jitless', 'd8'],
  [10, 'ignition', 'slow_path', 'd8'],
  [5, 'ignition', 'slow_path_opt', 'd8'],
  [25, 'ignition', 'ignition_turbo', 'd8'],
  [2, 'ignition_no_ic', 'ignition_turbo', 'd8'],
  [2, 'ignition', 'ignition_turbo_no_ic', 'd8'],
  [15, 'ignition', 'ignition_turbo_opt', 'd8'],
  [2, 'ignition_no_ic', 'ignition_turbo_opt', 'd8'],
  [4, 'ignition_turbo_opt', 'ignition_turbo_opt', 'clang_x64_pointer_compression/d8'],
  [5, 'ignition_turbo', 'ignition_turbo', 'clang_x64_pointer_compression/d8'],
  [4, 'ignition_turbo_opt', 'ignition_turbo_opt', 'clang_x86/d8'],
  [4, 'ignition_turbo', 'ignition_turbo', 'clang_x86/d8'],
  [4, 'ignition', 'ignition', 'clang_x86/d8'],
  [4, 'ignition', 'ignition', 'clang_x64_v8_arm64/d8'],
  [4, 'ignition', 'ignition', 'clang_x86_v8_arm/d8'],
]

# Additional flag experiments. List of tuples like
# (<likelihood to use flags in [0,1)>, <flag>).
ADDITIONAL_FLAGS = [
  (0.1, '--stress-marking=100'),
  (0.1, '--stress-scavenge=100'),
  (0.1, '--stress-compaction-random'),
  (0.1, '--random-gc-interval=2000'),
  (0.2, '--noanalyze-environment-liveness'),
  (0.1, '--stress-delay-tasks'),
  (0.01, '--thread-pool-size=1'),
  (0.01, '--thread-pool-size=2'),
  (0.01, '--thread-pool-size=4'),
  (0.01, '--thread-pool-size=8'),
  (0.1, '--interrupt-budget=1000'),
  (0.25, '--future'),
  (0.2, '--no-regexp-tier-up'),
  (0.1, '--regexp-interpret-all'),
  (0.1, '--regexp-tier-up-ticks=10'),
  (0.1, '--regexp-tier-up-ticks=100'),
  (0.1, '--turbo-instruction-scheduling'),
  (0.1, '--turbo-stress-instruction-scheduling'),
  (0.1, '--no-enable-sse3'),
  (0.1, '--no-enable-ssse3'),
  (0.1, '--no-enable-sse4_1'),
  (0.1, '--no-enable-sse4_2'),
  (0.1, '--no-enable-sahf'),
  (0.1, '--no-enable-avx'),
  (0.1, '--no-enable-fma3'),
  (0.1, '--no-enable-bmi1'),
  (0.1, '--no-enable-bmi2'),
  (0.1, '--no-enable-lzcnt'),
  (0.1, '--no-enable-popcnt'),
]

class Config(object):
  def __init__(self, name, rng=None):
    """
    Args:
      name: Name of the used fuzzer.
      rng: Random number generator for generating experiments.
      random_seed: Random-seed used for d8 throughout one fuzz session.
    """
    self.name = name
    self.rng = rng or random.Random()

  def choose_foozzie_flags(self):
    """Randomly chooses a configuration from FOOZZIE_EXPERIMENTS.

    Returns: List of flags to pass to v8_foozzie.py fuzz harness.
    """

    # Add additional flags to second config based on experiment percentages.
    extra_flags = []
    for p, flag in ADDITIONAL_FLAGS:
      if self.rng.random() < p:
        extra_flags.append('--second-config-extra-flags=%s' % flag)

    # Calculate flags determining the experiment.
    acc = 0
    threshold = self.rng.random() * 100
    for prob, first_config, second_config, second_d8 in FOOZZIE_EXPERIMENTS:
      acc += prob
      if acc > threshold:
        return [
          '--first-config=' + first_config,
          '--second-config=' + second_config,
          '--second-d8=' + second_d8,
        ] + extra_flags
    assert False
