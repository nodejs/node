# Copyright 2016 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

from testrunner.local import testsuite
from testrunner.local import utils
from testrunner.objects import testcase
from testrunner.outproc import base as outproc

PROTOCOL_TEST_JS = "protocol-test.js"
WASM_INSPECTOR_JS = "wasm-inspector-test.js"
PRIVATE_MEMBER_TEST_JS = "private-class-member-inspector-test.js"
EXPECTED_SUFFIX = "-expected.txt"
RESOURCES_FOLDER = "resources"


class TestLoader(testsuite.JSTestLoader):
  @property
  def excluded_files(self):
    return {PROTOCOL_TEST_JS, WASM_INSPECTOR_JS, PRIVATE_MEMBER_TEST_JS}

  @property
  def excluded_dirs(self):
    return {RESOURCES_FOLDER}


class TestSuite(testsuite.TestSuite):
  def _test_loader_class(self):
    return TestLoader

  def _test_class(self):
    return TestCase


class TestCase(testcase.TestCase):
  def __init__(self, *args, **kwargs):
    super(TestCase, self).__init__(*args, **kwargs)

    self._source_flags = self._parse_source_flags()

  def _get_files_params(self):
    return [
      self.suite.root / PROTOCOL_TEST_JS,
      self.suite.root / self.path_js,
    ]

  def _get_source_flags(self):
    return self._source_flags

  def _get_source_path(self):
    return self.suite.root / self.path_js

  def get_shell(self):
    return 'inspector-test'

  def get_android_resources(self):
    super_resources = super().get_android_resources()
    return super_resources + [
        self.suite.root /'debugger' /'resources' /'break-locations.js',
        self.suite.root / WASM_INSPECTOR_JS,
    ]

  @property
  def output_proc(self):
    return outproc.ExpectedOutProc(
        self.expected_outcomes,
        self.suite.root / self.path_and_suffix(EXPECTED_SUFFIX),
        self.test_config.regenerate_expected_files)
