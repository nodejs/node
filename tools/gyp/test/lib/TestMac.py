# Copyright (c) 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
TestMac.py:  a collection of helper function shared between test on Mac OS X.
"""

import re
import subprocess

__all__ = ['Xcode', 'CheckFileType']


def CheckFileType(test, file, archs):
  """Check that |file| contains exactly |archs| or fails |test|."""
  proc = subprocess.Popen(['lipo', '-info', file], stdout=subprocess.PIPE)
  o = proc.communicate()[0].strip()
  assert not proc.returncode
  if len(archs) == 1:
    pattern = re.compile('^Non-fat file: (.*) is architecture: (.*)$')
  else:
    pattern = re.compile('^Architectures in the fat file: (.*) are: (.*)$')
  match = pattern.match(o)
  if match is None:
    print 'Ouput does not match expected pattern: %s' % (pattern.pattern)
    test.fail_test()
  else:
    found_file, found_archs = match.groups()
    if found_file != file or set(found_archs.split()) != set(archs):
      print 'Expected file %s with arch %s, got %s with arch %s' % (
          file, ' '.join(archs), found_file, found_archs)
      test.fail_test()


class XcodeInfo(object):
  """Simplify access to Xcode informations."""

  def __init__(self):
    self._cache = {}

  def _XcodeVersion(self):
    lines = subprocess.check_output(['xcodebuild', '-version']).splitlines()
    version = ''.join(lines[0].split()[-1].split('.'))
    version = (version + '0' * (3 - len(version))).zfill(4)
    return version, lines[-1].split()[-1]

  def Version(self):
    if 'Version' not in self._cache:
      self._cache['Version'], self._cache['Build'] = self._XcodeVersion()
    return self._cache['Version']

  def Build(self):
    if 'Build' not in self._cache:
      self._cache['Version'], self._cache['Build'] = self._XcodeVersion()
    return self._cache['Build']

  def SDKBuild(self):
    if 'SDKBuild' not in self._cache:
      self._cache['SDKBuild'] = subprocess.check_output(
          ['xcodebuild', '-version', '-sdk', '', 'ProductBuildVersion'])
      self._cache['SDKBuild'] = self._cache['SDKBuild'].rstrip('\n')
    return self._cache['SDKBuild']

  def SDKVersion(self):
    if 'SDKVersion' not in self._cache:
      self._cache['SDKVersion'] = subprocess.check_output(
          ['xcodebuild', '-version', '-sdk', '', 'SDKVersion'])
      self._cache['SDKVersion'] = self._cache['SDKVersion'].rstrip('\n')
    return self._cache['SDKVersion']


Xcode = XcodeInfo()
