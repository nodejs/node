"""
Verifies that ios app bundles are built correctly.
"""

import subprocess

import TestGyp
import XCodeDetect

test = TestGyp.TestGyp(formats=['xcode', 'ninja'], platforms=['darwin'])

if not XCodeDetect.XCodeDetect.HasIPhoneSDK():
  test.skip_test('IPhone SDK not installed')


def CheckFileXMLPropertyList(file):
  output = subprocess.check_output(['file', file])
  if not 'XML 1.0 document text' in output:
    print('File: Expected XML 1.0 document text, got %s' % output)
    test.fail_test()


def CheckFileBinaryPropertyList(file):
  output = subprocess.check_output(['file', file])
  if not 'Apple binary property list' in output:
    print('File: Expected Apple binary property list, got %s' % output)
    test.fail_test()


test.run_gyp('test.gyp', chdir='app-bundle')

test.build('test.gyp', test.ALL, chdir='app-bundle')

# Test that the extension is .bundle
test.built_file_must_exist('Test App Gyp.app/Test App Gyp', chdir='app-bundle')

# Info.plist
info_plist = test.built_file_path('Test App Gyp.app/Info.plist', chdir='app-bundle')
test.built_file_must_exist(info_plist)
CheckFileBinaryPropertyList(info_plist)

# XML Info.plist
info_plist = test.built_file_path('Test App Gyp XML.app/Info.plist', chdir='app-bundle')
CheckFileXMLPropertyList(info_plist)

# Resources
strings_file = test.built_file_path('Test App Gyp.app/English.lproj/InfoPlist.strings', chdir='app-bundle')
test.built_file_must_exist(strings_file)
CheckFileBinaryPropertyList(strings_file)

extra_plist_file = test.built_file_path('Test App Gyp.app/English.lproj/LanguageMap.plist', chdir='app-bundle')
test.built_file_must_exist(extra_plist_file)
CheckFileBinaryPropertyList(extra_plist_file)

test.built_file_must_exist('Test App Gyp.app/English.lproj/MainMenu.nib', chdir='app-bundle')
test.built_file_must_exist('Test App Gyp.app/English.lproj/Main_iPhone.storyboardc', chdir='app-bundle')

# Packaging
test.built_file_must_exist('Test App Gyp.app/PkgInfo', chdir='app-bundle')
test.built_file_must_match('Test App Gyp.app/PkgInfo', 'APPLause', chdir='app-bundle')

test.pass_test()
