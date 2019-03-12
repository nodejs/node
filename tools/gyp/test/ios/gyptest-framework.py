"""
Verifies that ios app frameworks are built correctly.
"""

import TestGyp
from XCodeDetect import XCodeDetect

test = TestGyp.TestGyp(formats=['ninja'], platforms=['darwin'])

if XCodeDetect.Version() < '0700':
  test.skip_test('Skip test on XCode < 0700')

if test.format == 'xcode-ninja':
  test.skip_test()

test.run_gyp('framework.gyp', chdir='framework')

test.build('framework.gyp', 'iOSFramework', chdir='framework')

test.built_file_must_exist('iOSFramework.framework/Headers/iOSFramework.h', chdir='framework')
test.built_file_must_exist('iOSFramework.framework/Headers/Thing.h', chdir='framework')
test.built_file_must_exist('iOSFramework.framework/iOSFramework', chdir='framework')

test.pass_test()
