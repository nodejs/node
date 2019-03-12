"""
Verifies that tools are built correctly.
"""

import TestGyp

test = TestGyp.TestGyp(formats=['ninja', 'xcode'], platforms=['darwin'])

with TestGyp.LocalEnv({'GYP_CROSSCOMPILE': '1'}):
  test.run_gyp('test-crosscompile.gyp', chdir='app-bundle')

test.set_configuration('Default')
test.build('test-crosscompile.gyp', 'TestHost', chdir='app-bundle')
result_file = test.built_file_path('TestHost', chdir='app-bundle')
test.must_exist(result_file)
TestGyp.CheckFileType_macOS(test, result_file, ['x86_64'])

test.pass_test()
