#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that device and simulator bundles are built correctly.
"""

import plistlib
import TestGyp
import os
import struct
import subprocess
import sys
import tempfile


def CheckFileType(file, expected):
  proc = subprocess.Popen(['lipo', '-info', file], stdout=subprocess.PIPE)
  o = proc.communicate()[0].strip()
  assert not proc.returncode
  if not expected in o:
    print 'File: Expected %s, got %s' % (expected, o)
    test.fail_test()

def HasCerts():
  # Because the bots do not have certs, don't check them if there are no
  # certs available.
  proc = subprocess.Popen(['security','find-identity','-p', 'codesigning',
                           '-v'], stdout=subprocess.PIPE)
  return "0 valid identities found" not in proc.communicate()[0].strip()

def CheckSignature(file):
  proc = subprocess.Popen(['codesign', '-v', file], stdout=subprocess.PIPE)
  o = proc.communicate()[0].strip()
  assert not proc.returncode
  if "code object is not signed at all" in o:
    print 'File %s not properly signed.' % (file)
    test.fail_test()

def CheckEntitlements(file, expected_entitlements):
  with tempfile.NamedTemporaryFile() as temp:
    proc = subprocess.Popen(['codesign', '--display', '--entitlements',
                             temp.name, file], stdout=subprocess.PIPE)
    o = proc.communicate()[0].strip()
    assert not proc.returncode
    data = temp.read()
  entitlements = ParseEntitlements(data)
  if not entitlements:
    print 'No valid entitlements found in %s.' % (file)
    test.fail_test()
  if entitlements != expected_entitlements:
    print 'Unexpected entitlements found in %s.' % (file)
    test.fail_test()

def ParseEntitlements(data):
  if len(data) < 8:
    return None
  magic, length = struct.unpack('>II', data[:8])
  if magic != 0xfade7171 or length != len(data):
    return None
  return data[8:]

def GetProductVersion():
  args = ['xcodebuild','-version','-sdk','iphoneos','ProductVersion']
  job = subprocess.Popen(args, stdout=subprocess.PIPE)
  return job.communicate()[0].strip()

def CheckPlistvalue(plist, key, expected):
  if key not in plist:
    print '%s not set in plist' % key
    test.fail_test()
    return
  actual = plist[key]
  if actual != expected:
    print 'File: Expected %s, got %s for %s' % (expected, actual, key)
    test.fail_test()

def CheckPlistNotSet(plist, key):
  if key in plist:
    print '%s should not be set in plist' % key
    test.fail_test()
    return

def ConvertBinaryPlistToXML(path):
  proc = subprocess.call(['plutil', '-convert', 'xml1', path],
                         stdout=subprocess.PIPE)

if sys.platform == 'darwin':
  test = TestGyp.TestGyp(formats=['ninja', 'xcode'])

  test.run_gyp('test-device.gyp', chdir='app-bundle')

  test_configs = ['Default-iphoneos', 'Default']
  # TODO(justincohen): Disabling 'Default-iphoneos' for xcode until bots are
  # configured with signing certs.
  if test.format == 'xcode':
    test_configs.remove('Default-iphoneos')

  for configuration in test_configs:
    test.set_configuration(configuration)
    test.build('test-device.gyp', 'test_app', chdir='app-bundle')
    result_file = test.built_file_path('Test App Gyp.bundle/Test App Gyp',
                                       chdir='app-bundle')
    test.must_exist(result_file)

    info_plist = test.built_file_path('Test App Gyp.bundle/Info.plist',
                                      chdir='app-bundle')

    # plistlib doesn't support binary plists, but that's what Xcode creates.
    if test.format == 'xcode':
      ConvertBinaryPlistToXML(info_plist)
    plist = plistlib.readPlist(info_plist)

    CheckPlistvalue(plist, 'UIDeviceFamily', [1, 2])

    if configuration == 'Default-iphoneos':
      CheckFileType(result_file, 'armv7')
      CheckPlistvalue(plist, 'DTPlatformVersion', GetProductVersion())
      CheckPlistvalue(plist, 'CFBundleSupportedPlatforms', ['iPhoneOS'])
      CheckPlistvalue(plist, 'DTPlatformName', 'iphoneos')
    else:
      CheckFileType(result_file, 'i386')
      CheckPlistNotSet(plist, 'DTPlatformVersion')
      CheckPlistvalue(plist, 'CFBundleSupportedPlatforms', ['iPhoneSimulator'])
      CheckPlistvalue(plist, 'DTPlatformName', 'iphonesimulator')

    if HasCerts() and configuration == 'Default-iphoneos':
      test.build('test-device.gyp', 'sig_test', chdir='app-bundle')
      result_file = test.built_file_path('sig_test.bundle/sig_test',
                                         chdir='app-bundle')
      CheckSignature(result_file)
      info_plist = test.built_file_path('sig_test.bundle/Info.plist',
                                        chdir='app-bundle')

      plist = plistlib.readPlist(info_plist)
      CheckPlistvalue(plist, 'UIDeviceFamily', [1])

      entitlements_file = test.built_file_path('sig_test.xcent',
                                               chdir='app-bundle')
      if os.path.isfile(entitlements_file):
        expected_entitlements = open(entitlements_file).read()
        CheckEntitlements(result_file, expected_entitlements)

  test.pass_test()
