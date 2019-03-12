#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

""" Unit tests for the ninja.py file. """

import unittest
import sys

import gyp.NinjaWriter


class TestPrefixesAndSuffixes(unittest.TestCase):
  spec = {'target_name': 'wee', 'type': 'executable', 'toolset': 'target', 'configurations': {'gu': {}, 'ga': {}}}
  def setUp(self):
    self.win_writer = gyp.NinjaWriter.NinjaWriter('foo', 'wee', '.', '.', 'build.ninja', '.', 'build.ninja', 'win', self.spec, {'msvs_version': '2019'}, 'gu', '.')
    self.linux_writer = gyp.NinjaWriter.NinjaWriter('foo', 'wee', '.', '.', 'build.ninja', '.', 'build.ninja', 'linux', self.spec, {'msvs_version': '2019'}, 'ga', '.')

  def test_BinaryNamesWindows(self):
    # These cannot run on non-Windows as they require a VS installation to
    # correctly handle variable expansion.
    if not sys.platform.startswith('win'):
      self.skipTest('can only run on windows')
    self.assertTrue(self.win_writer.ComputeOutputFileName(self.spec, 'executable').endswith('.exe'))
    self.assertTrue(self.win_writer.ComputeOutputFileName(self.spec, 'shared_library').endswith('.dll'))
    self.assertTrue(self.win_writer.ComputeOutputFileName(self.spec, 'static_library').endswith('.lib'))

  def test_BinaryNamesLinux(self):
    self.assertTrue('.' not in self.linux_writer.ComputeOutputFileName(self.spec, 'executable'))
    self.assertTrue(self.linux_writer.ComputeOutputFileName(self.spec, 'shared_library').startswith('lib'))
    self.assertTrue(self.linux_writer.ComputeOutputFileName(self.spec, 'static_library').startswith('lib'))
    self.assertTrue(self.linux_writer.ComputeOutputFileName(self.spec, 'shared_library').endswith('.so'))
    self.assertTrue(self.linux_writer.ComputeOutputFileName(self.spec, 'static_library').endswith('.a'))


if __name__ == '__main__':
  unittest.main()
