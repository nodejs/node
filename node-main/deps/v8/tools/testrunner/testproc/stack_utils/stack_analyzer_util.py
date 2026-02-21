#!/usr/bin/env python3
# Copyright 2022 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import re
import sys


ANALYSIS_FAILURE_STATES = ['NULL', 'Unknown', '', None]
STACKFRAME_REGEX = r'^#\d+.*'
FILE_LOCATION_REGEX = r'.*\:\d+\:\d+$'


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

    from clusterfuzz.stacktraces import StackParser, MAX_CRASH_STATE_FRAMES
    from clusterfuzz.stacktraces import llvm_test_one_input_override
    from clusterfuzz.stacktraces import constants

    self.MAX_CRASH_STATE_FRAMES = MAX_CRASH_STATE_FRAMES
    self.stack_parser = StackParser()
    self.constants = constants
    self.llvm_test_one_input_override = llvm_test_one_input_override

  def analyze_crash(self, stderr):
    try:
      stderr_analysis = self.stack_parser.parse(stderr)
      self.custom_analyzer(stderr_analysis)
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

  def _extract_function_name(self, frame):
    split_frame = frame.split()
    start_loc = 1
    end_loc = -1
    if len(split_frame) > 2 and split_frame[2] == 'in':
      start_loc = 3
    split_frame = split_frame[start_loc:]
    for idx, item in enumerate(split_frame):
      # exclude file name and everything after it
      if re.match(FILE_LOCATION_REGEX, item):
        end_loc = idx
        break
    if end_loc != -1:
      return ' '.join(split_frame[:end_loc])
    return None

  def _fallback_crash_state(self, stacktrace):
    status = []
    for line in [l.strip() for l in stacktrace.splitlines()]:
      if re.match(STACKFRAME_REGEX, line):
        frame = self._extract_function_name(line)
        if not self.stack_parser.ignore_stack_frame(frame):
          status.append(frame)
        if len(status) >= self.MAX_CRASH_STATE_FRAMES:
          break

    return '\n'.join(status)

  def custom_analyzer(self, crash_info):
    if crash_info.crash_state in ANALYSIS_FAILURE_STATES:
      fallback_state = self._fallback_crash_state(crash_info.crash_stacktrace)
      crash_info.crash_state = fallback_state or crash_info.crash_state


class EmptyStackParser:

  def analyze_crash(self, stderr):
    return {}


def create_stack_parser():
  try:
    return CustomStackParser()
  except ImportError as e:
    logging.info(e)
    return EmptyStackParser()
