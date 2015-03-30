#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that dependent Xcode settings are processed correctly.
"""

import TestGyp
import TestMac

import subprocess
import sys

if sys.platform == 'darwin':
  test = TestGyp.TestGyp(formats=['ninja', 'make', 'xcode'])

  CHDIR = 'xcode-env-order'
  INFO_PLIST_PATH = 'Test.app/Contents/Info.plist'

  test.run_gyp('test.gyp', chdir=CHDIR)
  test.build('test.gyp', test.ALL, chdir=CHDIR)

  # Env vars in 'copies' filenames.
  test.built_file_must_exist('Test-copy-brace/main.c', chdir=CHDIR)
  test.built_file_must_exist('Test-copy-paren/main.c', chdir=CHDIR)
  test.built_file_must_exist('Test-copy-bare/main.c', chdir=CHDIR)

  # Env vars in 'actions' filenames and inline actions
  test.built_file_must_exist('action-copy-brace.txt', chdir=CHDIR)
  test.built_file_must_exist('action-copy-paren.txt', chdir=CHDIR)
  test.built_file_must_exist('action-copy-bare.txt', chdir=CHDIR)

  # Env vars in 'rules' filenames and inline actions
  test.built_file_must_exist('rule-copy-brace.txt', chdir=CHDIR)
  test.built_file_must_exist('rule-copy-paren.txt', chdir=CHDIR)
  # TODO: see comment in test.gyp for this file.
  #test.built_file_must_exist('rule-copy-bare.txt', chdir=CHDIR)

  # Env vars in Info.plist.
  info_plist = test.built_file_path(INFO_PLIST_PATH, chdir=CHDIR)
  test.must_exist(info_plist)

  test.must_contain(info_plist, '''\
\t<key>BraceProcessedKey1</key>
\t<string>D:/Source/Project/Test</string>''')
  test.must_contain(info_plist, '''\
\t<key>BraceProcessedKey2</key>
\t<string>/Source/Project/Test</string>''')
  test.must_contain(info_plist, '''\
\t<key>BraceProcessedKey3</key>
\t<string>com.apple.product-type.application:D:/Source/Project/Test</string>''')

  test.must_contain(info_plist, '''\
\t<key>ParenProcessedKey1</key>
\t<string>D:/Source/Project/Test</string>''')
  test.must_contain(info_plist, '''\
\t<key>ParenProcessedKey2</key>
\t<string>/Source/Project/Test</string>''')
  test.must_contain(info_plist, '''\
\t<key>ParenProcessedKey3</key>
\t<string>com.apple.product-type.application:D:/Source/Project/Test</string>''')

  test.must_contain(info_plist, '''\
\t<key>BareProcessedKey1</key>
\t<string>D:/Source/Project/Test</string>''')
  test.must_contain(info_plist, '''\
\t<key>BareProcessedKey2</key>
\t<string>/Source/Project/Test</string>''')
  # NOTE: For bare variables, $PRODUCT_TYPE is not replaced! It _is_ replaced
  # if it's not right at the start of the string (e.g. ':$PRODUCT_TYPE'), so
  # this looks like an Xcode bug. This bug isn't emulated (yet?), so check this
  # only for Xcode.
  if test.format == 'xcode' and TestMac.Xcode.Version() < '0500':
    test.must_contain(info_plist, '''\
\t<key>BareProcessedKey3</key>
\t<string>$PRODUCT_TYPE:D:/Source/Project/Test</string>''')
  else:
    # The bug has been fixed by Xcode version 5.0.0.
    test.must_contain(info_plist, '''\
\t<key>BareProcessedKey3</key>
\t<string>com.apple.product-type.application:D:/Source/Project/Test</string>''')

  test.must_contain(info_plist, '''\
\t<key>MixedProcessedKey</key>
\t<string>/Source/Project:Test:mh_execute</string>''')

  test.pass_test()
