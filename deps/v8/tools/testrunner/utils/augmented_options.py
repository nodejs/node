# Copyright 2022 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import optparse
import os
import random

from functools import cached_property

from testrunner.testproc import fuzzer


class AugmentedOptions(optparse.Values):
  """This class will augment exiting options object with
  a couple of convenient methods and properties.
  """
  @staticmethod
  def augment(options_object):
    options_object.__class__ = AugmentedOptions
    return options_object

  def fuzzer_rng(self):
    if not getattr(self,'_fuzzer_rng', None):
      self._fuzzer_rng = random.Random(self.fuzzer_random_seed)
    return self._fuzzer_rng

  @cached_property
  def shard_info(self):
    """
    Returns pair:
      (id of the current shard [1; number of shards], number of shards)
    """
    # Read gtest shard configuration from environment (e.g. set by swarming).
    # If none is present, use values passed on the command line.
    count = int(
      os.environ.get('GTEST_TOTAL_SHARDS', self.shard_count))
    run = os.environ.get('GTEST_SHARD_INDEX')
    # The v8 shard_run starts at 1, while GTEST_SHARD_INDEX starts at 0.
    run = int(run) + 1 if run else self.shard_run

    if self.shard_count > 1:
      # Log if a value was passed on the cmd line and it differs from the
      # environment variables.
      if self.shard_count != count:  # pragma: no cover
        print("shard_count from cmd line differs from environment variable "
              "GTEST_TOTAL_SHARDS")
      if (self.shard_run > 1 and
          self.shard_run != run):  # pragma: no cover
        print("shard_run from cmd line differs from environment variable "
              "GTEST_SHARD_INDEX")

    if run < 1 or run > count:
      # TODO(machenbach): Turn this into an assert. If that's wrong on the
      # bots, printing will be quite useless. Or refactor this code to make
      # sure we get a return code != 0 after testing if we got here.
      print("shard-run not a valid number, should be in [1:shard-count]")
      print("defaulting back to running all tests")
      return 0, 1

    return run - 1, count # coming back to 0 based counting

  def fuzzer_configs(self):
    fuzzers = []
    def add(name, prob):
      if prob:
        fuzzers.append(fuzzer.create_fuzzer_config(name, prob))

    add('allocation', self.allocation_offset)
    add('compaction', self.stress_compaction)
    add('interrupt', self.stress_interrupt_budget)
    add('marking', self.stress_marking)
    add('scavenge', self.stress_scavenge)
    add('gc_interval', self.stress_gc)
    add('stack', self.stress_stack_size)
    add('threads', self.stress_thread_pool_size)
    add('delay', self.stress_delay_tasks)
    add('deopt', self.stress_deopt)
    return fuzzers

  def fuzzer_tests_count(self):
    if self.combine_tests:
      return 1
    return self.tests_count
