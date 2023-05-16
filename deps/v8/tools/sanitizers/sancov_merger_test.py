# Copyright 2016 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import sancov_merger


# Files on disk after test runner completes. The files are mapped by
# executable name -> file list.
FILE_MAP = {
  'd8': [
    'd8.test.1.1.sancov',
    'd8.test.2.1.sancov',
    'd8.test.3.1.sancov',
    'd8.test.4.1.sancov',
    'd8.test.5.1.sancov',
    'd8.test.5.2.sancov',
    'd8.test.6.1.sancov',
  ],
  'cctest': [
    'cctest.test.1.1.sancov',
    'cctest.test.2.1.sancov',
    'cctest.test.3.1.sancov',
    'cctest.test.4.1.sancov',
  ],
}


# Inputs for merge process with 2 cpus. The tuples contain:
# (flag, path, executable name, intermediate result index, file list).
EXPECTED_INPUTS_2 = [
  (False, '/some/path', 'cctest', 0, [
    'cctest.test.1.1.sancov',
    'cctest.test.2.1.sancov']),
  (False, '/some/path', 'cctest', 1, [
    'cctest.test.3.1.sancov',
    'cctest.test.4.1.sancov']),
  (False, '/some/path', 'd8', 0, [
    'd8.test.1.1.sancov',
    'd8.test.2.1.sancov',
    'd8.test.3.1.sancov',
    'd8.test.4.1.sancov']),
  (False, '/some/path', 'd8', 1, [
    'd8.test.5.1.sancov',
    'd8.test.5.2.sancov',
    'd8.test.6.1.sancov']),
]


# The same for 4 cpus.
EXPECTED_INPUTS_4 = [
  (True, '/some/path', 'cctest', 0, [
    'cctest.test.1.1.sancov',
    'cctest.test.2.1.sancov']),
  (True, '/some/path', 'cctest', 1, [
    'cctest.test.3.1.sancov',
    'cctest.test.4.1.sancov']),
  (True, '/some/path', 'd8', 0, [
    'd8.test.1.1.sancov',
    'd8.test.2.1.sancov']),
  (True, '/some/path', 'd8', 1, [
    'd8.test.3.1.sancov',
    'd8.test.4.1.sancov']),
  (True, '/some/path', 'd8', 2, [
    'd8.test.5.1.sancov',
    'd8.test.5.2.sancov']),
  (True, '/some/path', 'd8', 3, [
    'd8.test.6.1.sancov'])]


class MergerTests(unittest.TestCase):
  def test_generate_inputs_2_cpu(self):
    inputs = sancov_merger.generate_inputs(
        False, '/some/path', FILE_MAP, 2)
    self.assertEquals(EXPECTED_INPUTS_2, inputs)

  def test_generate_inputs_4_cpu(self):
    inputs = sancov_merger.generate_inputs(
        True, '/some/path', FILE_MAP, 4)
    self.assertEquals(EXPECTED_INPUTS_4, inputs)
