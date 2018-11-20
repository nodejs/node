# Copyright 2018 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import random

from .utils import random_utils


class TestConfig(object):
  def __init__(self,
               command_prefix,
               extra_flags,
               isolates,
               mode_flags,
               no_harness,
               noi18n,
               random_seed,
               shell_dir,
               timeout,
               verbose):
    self.command_prefix = command_prefix
    self.extra_flags = extra_flags
    self.isolates = isolates
    self.mode_flags = mode_flags
    self.no_harness = no_harness
    self.noi18n = noi18n
    # random_seed is always not None.
    self.random_seed = random_seed or random_utils.random_seed()
    self.shell_dir = shell_dir
    self.timeout = timeout
    self.verbose = verbose
