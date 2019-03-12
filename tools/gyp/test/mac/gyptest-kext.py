"""
Verifies that kext bundles are built correctly.
"""

import TestGyp

test = TestGyp.TestGyp(formats=['xcode'], platforms=['darwin'])
test.run_gyp('kext.gyp', chdir='kext')
test.build('kext.gyp', test.ALL, chdir='kext')
test.built_file_must_exist('GypKext.kext/Contents/MacOS/GypKext', chdir='kext')
test.built_file_must_exist('GypKext.kext/Contents/Info.plist', chdir='kext')
test.pass_test()
