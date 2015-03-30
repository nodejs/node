#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that app bundles are built correctly.
"""

import TestGyp
import TestMac

import os
import plistlib
import subprocess
import sys


def ExpectEq(expected, actual):
  if expected != actual:
    print >>sys.stderr, 'Expected "%s", got "%s"' % (expected, actual)
    test.fail_test()

def ls(path):
  '''Returns a list of all files in a directory, relative to the directory.'''
  result = []
  for dirpath, _, files in os.walk(path):
    for f in files:
      result.append(os.path.join(dirpath, f)[len(path) + 1:])
  return result

# Xcode supports for assets catalog was introduced in Xcode 6.0
if sys.platform == 'darwin' and TestMac.Xcode.Version() >= '0600':
  test_gyp_path = 'test-assets-catalog.gyp'
  test_app_path = 'Test App Assets Catalog Gyp.app'

  test = TestGyp.TestGyp(formats=['ninja', 'make', 'xcode'])
  test.run_gyp(test_gyp_path, chdir='app-bundle')
  test.build(test_gyp_path, test.ALL, chdir='app-bundle')

  # Binary
  test.built_file_must_exist(
      os.path.join(test_app_path, 'Contents/MacOS/Test App Assets Catalog Gyp'),
      chdir='app-bundle')

  # Info.plist
  info_plist = test.built_file_path(
      os.path.join(test_app_path, 'Contents/Info.plist'),
      chdir='app-bundle')
  test.must_exist(info_plist)
  test.must_contain(
      info_plist,
      'com.google.Test-App-Assets-Catalog-Gyp')  # Variable expansion
  test.must_not_contain(info_plist, '${MACOSX_DEPLOYMENT_TARGET}');

  if test.format != 'make':
    # TODO: Synthesized plist entries aren't hooked up in the make generator.
    machine = subprocess.check_output(['sw_vers', '-buildVersion']).rstrip('\n')
    plist = plistlib.readPlist(info_plist)
    ExpectEq(machine, plist['BuildMachineOSBuild'])

    expected = ''
    version = TestMac.Xcode.SDKVersion()
    expected = 'macosx' + version
    ExpectEq(expected, plist['DTSDKName'])
    sdkbuild = TestMac.Xcode.SDKBuild()
    if not sdkbuild:
      # Above command doesn't work in Xcode 4.2.
      sdkbuild = plist['BuildMachineOSBuild']
    ExpectEq(sdkbuild, plist['DTSDKBuild'])
    ExpectEq(TestMac.Xcode.Version(), plist['DTXcode'])
    ExpectEq(TestMac.Xcode.Build(), plist['DTXcodeBuild'])

  # Resources
  strings_files = ['InfoPlist.strings', 'utf-16be.strings', 'utf-16le.strings']
  for f in strings_files:
    strings = test.built_file_path(
        os.path.join(test_app_path, 'Contents/Resources/English.lproj', f),
        chdir='app-bundle')
    test.must_exist(strings)
    # Xcodes writes UTF-16LE with BOM.
    contents = open(strings, 'rb').read()
    if not contents.startswith('\xff\xfe' + '/* Localized'.encode('utf-16le')):
      test.fail_test()

  test.built_file_must_exist(
      os.path.join(
          test_app_path, 'Contents/Resources/English.lproj/MainMenu.nib'),
      chdir='app-bundle')

  # make does not supports .xcassets files
  extra_content_files = []
  if test.format != 'make':
    extra_content_files = ['Contents/Resources/Assets.car']
    for f in extra_content_files:
      test.built_file_must_exist(
          os.path.join(test_app_path, f),
          chdir='app-bundle')

  # Packaging
  test.built_file_must_exist(
      os.path.join(test_app_path, 'Contents/PkgInfo'),
      chdir='app-bundle')
  test.built_file_must_match(
      os.path.join(test_app_path, 'Contents/PkgInfo'), 'APPLause',
      chdir='app-bundle')

  # Check that no other files get added to the bundle.
  if set(ls(test.built_file_path(test_app_path, chdir='app-bundle'))) != \
     set(['Contents/MacOS/Test App Assets Catalog Gyp',
          'Contents/Info.plist',
          'Contents/Resources/English.lproj/MainMenu.nib',
          'Contents/PkgInfo',
          ] + extra_content_files +
         [os.path.join('Contents/Resources/English.lproj', f)
             for f in strings_files]):
    test.fail_test()

  test.pass_test()
