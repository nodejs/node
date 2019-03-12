#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that device and simulator bundles are built correctly.
"""

from __future__ import print_function

import os
import plistlib
import struct
import subprocess
import tempfile

import TestGyp
from XCodeDetect import XCodeDetect

test = TestGyp.TestGyp(formats=['ninja', 'xcode'], platforms=['darwin'], disable="This test is currently disabled: https://crbug.com/483696.")


def CheckFileType(file, expected):
  proc = subprocess.Popen(['lipo', '-info', file], stdout=subprocess.PIPE)
  o = proc.communicate()[0].strip()
  assert not proc.returncode
  if not expected in o:
    print('File: Expected %s, got %s' % (expected, o))
    test.fail_test()


def HasCerts():
  # Because the bots do not have certs, don't check them if there are no
  # certs available.
  proc = subprocess.Popen(['security', 'find-identity', '-p', 'codesigning', '-v'], stdout=subprocess.PIPE)
  return "0 valid identities found" not in proc.communicate()[0].strip()


def CheckSignature(file):
  proc = subprocess.Popen(['codesign', '-v', file], stdout=subprocess.PIPE)
  o = proc.communicate()[0].strip()
  assert not proc.returncode
  if "code object is not signed at all" in o:
    print('File %s not properly signed.' % (file))
    test.fail_test()


def CheckEntitlements(file, expected_entitlements):
  with tempfile.NamedTemporaryFile() as temp:
    proc = subprocess.Popen(['codesign', '--display', '--entitlements', temp.name, file], stdout=subprocess.PIPE)
    proc.wait()
    assert not proc.returncode
    data = temp.read()
  entitlements = ParseEntitlements(data)
  if not entitlements:
    print('No valid entitlements found in %s.' % (file))
    test.fail_test()
  if entitlements != expected_entitlements:
    print('Unexpected entitlements found in %s.' % (file))
    test.fail_test()


def ParseEntitlements(data):
  if len(data) < 8:
    return None
  magic, length = struct.unpack('>II', data[:8])
  if magic != 0xfade7171 or length != len(data):
    return None
  return data[8:]


def GetXcodeVersionValue(type):
  args = ['xcodebuild', '-version', '-sdk', 'iphoneos', type]
  job = subprocess.Popen(args, stdout=subprocess.PIPE)
  return job.communicate()[0].strip()


def GetMachineBuild():
  args = ['sw_vers', '-buildVersion']
  job = subprocess.Popen(args, stdout=subprocess.PIPE)
  return job.communicate()[0].strip()


def CheckPlistvalue(plist, key, expected):
  if key not in plist:
    print('%s not set in plist' % key)
    test.fail_test()
    return
  actual = plist[key]
  if actual != expected:
    print('File: Expected %s, got %s for %s' % (expected, actual, key))
    test.fail_test()


def CheckPlistNotSet(plist, key):
  if key in plist:
    print('%s should not be set in plist' % key)
    test.fail_test()
    return


def ConvertBinaryPlistToXML(path):
  subprocess.call(['plutil', '-convert', 'xml1', path], stdout=subprocess.PIPE)


test.run_gyp('test-device.gyp', chdir='app-bundle')

test_configs = ['Default-iphoneos', 'Default']
for configuration in test_configs:
  test.set_configuration(configuration)
  test.build('test-device.gyp', 'test_app', chdir='app-bundle')
  result_file = test.built_file_path('Test App Gyp.app/Test App Gyp', chdir='app-bundle')
  test.must_exist(result_file)
  info_plist = test.built_file_path('Test App Gyp.app/Info.plist', chdir='app-bundle')
  plist = plistlib.readPlist(info_plist)
  xcode_version = XCodeDetect.Version()
  if xcode_version >= '0720':
    if len(plist) != 23:
      print('plist should have 23 entries, but it has %s' % len(plist))
      test.fail_test()

  # Values that will hopefully never change.
  CheckPlistvalue(plist, 'CFBundleDevelopmentRegion', 'English')
  CheckPlistvalue(plist, 'CFBundleExecutable', 'Test App Gyp')
  CheckPlistvalue(plist, 'CFBundleIdentifier', 'com.google.Test App Gyp')
  CheckPlistvalue(plist, 'CFBundleInfoDictionaryVersion', '6.0')
  CheckPlistvalue(plist, 'CFBundleName', 'Test App Gyp')
  CheckPlistvalue(plist, 'CFBundlePackageType', 'APPL')
  CheckPlistvalue(plist, 'CFBundleShortVersionString', '1.0')
  CheckPlistvalue(plist, 'CFBundleSignature', 'ause')
  CheckPlistvalue(plist, 'CFBundleVersion', '1')
  CheckPlistvalue(plist, 'NSMainNibFile', 'MainMenu')
  CheckPlistvalue(plist, 'NSPrincipalClass', 'NSApplication')
  CheckPlistvalue(plist, 'UIDeviceFamily', [1, 2])

  # Values that get pulled from xcodebuild.
  machine_build = GetMachineBuild()
  platform_version = GetXcodeVersionValue('ProductVersion')
  sdk_build = GetXcodeVersionValue('ProductBuildVersion')

  # Xcode keeps changing what gets included in executable plists, and it
  # changes between device and simuator builds.  Allow the strictest tests for
  # Xcode 7.2 and above.
  if xcode_version >= '0720':
    CheckPlistvalue(plist, 'BuildMachineOSBuild', machine_build)
    CheckPlistvalue(plist, 'DTCompiler', 'com.apple.compilers.llvm.clang.1_0')
    CheckPlistvalue(plist, 'DTPlatformVersion', platform_version)
    CheckPlistvalue(plist, 'DTSDKBuild', sdk_build)
    CheckPlistvalue(plist, 'DTXcode', xcode_version)
    CheckPlistvalue(plist, 'MinimumOSVersion', '8.0')

  if configuration == 'Default-iphoneos':
    platform_name = 'iphoneos'
    CheckFileType(result_file, 'armv7')
    CheckPlistvalue(plist, 'CFBundleSupportedPlatforms', ['iPhoneOS'])
    # Apple keeps changing their mind.
    if xcode_version >= '0720':
      CheckPlistvalue(plist, 'DTPlatformBuild', sdk_build)
  else:
    platform_name = 'iphonesimulator'
    CheckFileType(result_file, 'i386')
    CheckPlistvalue(plist, 'CFBundleSupportedPlatforms', ['iPhoneSimulator'])
    if xcode_version >= '0720':
      CheckPlistvalue(plist, 'DTPlatformBuild', '')

  CheckPlistvalue(plist, 'DTPlatformName', platform_name)
  CheckPlistvalue(plist, 'DTSDKName', platform_name + platform_version)

  if HasCerts() and configuration == 'Default-iphoneos':
    test.build('test-device.gyp', 'sig_test', chdir='app-bundle')
    result_file = test.built_file_path('sigtest.app/sigtest', chdir='app-bundle')
    CheckSignature(result_file)
    info_plist = test.built_file_path('sigtest.app/Info.plist', chdir='app-bundle')

    plist = plistlib.readPlist(info_plist)
    CheckPlistvalue(plist, 'UIDeviceFamily', [1])

    entitlements_file = test.built_file_path('sig_test.xcent', chdir='app-bundle')
    if os.path.isfile(entitlements_file):
      expected_entitlements = open(entitlements_file).read()
      CheckEntitlements(result_file, expected_entitlements)

test.pass_test()
