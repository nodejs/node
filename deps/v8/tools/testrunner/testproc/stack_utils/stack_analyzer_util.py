#!/usr/bin/env python3
# Copyright 2022 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import sys


def fake_clusterfuzz_imports(modules):
  for module in modules:
    sys.modules[module] = __import__('clusterfuzz_fakes')


def pre_clusterfuzz_import():
  local_path = os.path.dirname(os.path.abspath(__file__))
  sys.path.append(local_path)

  fake_clusterfuzz_imports([
      'clusterfuzz._internal.platforms.android',
      'clusterfuzz._internal.google_cloud_utils',
      'clusterfuzz._internal.config.local_config',
  ])


class CustomStackParser:

  def __init__(self):
    pre_clusterfuzz_import()

    from clusterfuzz.stacktraces import StackParser
    from clusterfuzz.stacktraces import llvm_test_one_input_override
    from clusterfuzz.stacktraces import constants

    self.stack_parser = StackParser()
    self.constants = constants
    self.llvm_test_one_input_override = llvm_test_one_input_override

  def analyze_crash(self, stderr):
    try:
      stderr_analysis = self.stack_parser.parse(stderr)
      return {
          "crash_state": stderr_analysis.crash_state.strip(),
          "crash_type": stderr_analysis.crash_type.strip(),
      }
    except Exception as e:
      logging.info(e)
      return {
          "crash_state": "Unknown",
          "crash_type": "Unknown",
      }


class EmptyStackParser:

  def analyze_crash(self, stderr):
    return {}


def create_stack_parser():
  try:
    return CustomStackParser()
  except ImportError as e:
    logging.info(e)
    return EmptyStackParser()
