"""
Verifies that ios watch extensions and apps are built correctly.
"""

from __future__ import print_function

import TestGyp
from XCodeDetect import XCodeDetect

test = TestGyp.TestGyp(formats=['ninja', 'xcode'], platforms=['darwin'], disable="This test is currently disabled: https://crbug.com/483696.")

if XCodeDetect.Version() < '0620':
  test.skip_test('Skip test on XCode < 0620')

test.run_gyp('watch.gyp', chdir='watch')

test.build(  'watch.gyp',  'WatchContainer',  chdir='watch')

# Test that the extension exists
test.built_file_must_exist(  'WatchContainer.app/PlugIns/WatchKitExtension.appex',  chdir='watch')

# Test that the watch app exists
test.built_file_must_exist(  'WatchContainer.app/PlugIns/WatchKitExtension.appex/WatchApp.app',  chdir='watch')

test.pass_test()
