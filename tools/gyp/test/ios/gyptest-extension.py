#!/usr/bin/env python

# Copyright (c) 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that ios app extensions are built correctly.
"""

from __future__ import print_function

import subprocess

import TestGyp
from XCodeDetect import XCodeDetect

test = TestGyp.TestGyp(formats=['ninja', 'xcode'], platforms=['darwin'])

if XCodeDetect.Version() < '0600':
  test.skip_test('Skip test on XCode < 0600')


def CheckStrip(p, expected):
  if expected not in subprocess.check_output(['nm', '-gU', p]):
    print(expected + " shouldn't get stripped out.")
    test.fail_test()


def CheckEntrypoint(p, expected):
  if expected not in subprocess.check_output(['nm', p]):
    print(expected + "not found.")
    test.fail_test()


if test.format in ('ninja', 'xcode-ninja'):
  test.skip_test()  # bug=534

test.run_gyp('extension.gyp', chdir='extension')

test.build('extension.gyp', 'ExtensionContainer', chdir='extension')

# Test that the extension is .appex
test.built_file_must_exist(
  'ExtensionContainer.app/PlugIns/ActionExtension.appex',
  chdir='extension')

path = test.built_file_path('ExtensionContainer.app/PlugIns/ActionExtension.appex/ActionExtension', chdir='extension')
CheckStrip(path, "ActionViewController")
CheckEntrypoint(path, "_NSExtensionMain")

test.pass_test()
