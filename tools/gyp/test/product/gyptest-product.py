#!/usr/bin/env python

# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies simplest-possible build of a "Hello, world!" program
using the default build target.
"""

import TestGyp

test = TestGyp.TestGyp()

test.run_gyp('product.gyp')
test.build('product.gyp')

# executables
test.built_file_must_exist('alt1' + test._exe, test.EXECUTABLE, bare=True)
test.built_file_must_exist('hello2.stuff', test.EXECUTABLE, bare=True)
test.built_file_must_exist('yoalt3.stuff', test.EXECUTABLE, bare=True)

# shared libraries
test.built_file_must_exist(test.dll_ + 'alt4' + test._dll,
                           test.SHARED_LIB, bare=True)
test.built_file_must_exist(test.dll_ + 'hello5.stuff',
                           test.SHARED_LIB, bare=True)
test.built_file_must_exist('yoalt6.stuff', test.SHARED_LIB, bare=True)

# static libraries
test.built_file_must_exist(test.lib_ + 'alt7' + test._lib,
                           test.STATIC_LIB, bare=True)
test.built_file_must_exist(test.lib_ + 'hello8.stuff',
                           test.STATIC_LIB, bare=True)
test.built_file_must_exist('yoalt9.stuff', test.STATIC_LIB, bare=True)

# alternate product_dir
test.built_file_must_exist('bob/yoalt10.stuff', test.EXECUTABLE, bare=True)
test.built_file_must_exist('bob/yoalt11.stuff', test.EXECUTABLE, bare=True)
test.built_file_must_exist('bob/yoalt12.stuff', test.EXECUTABLE, bare=True)

test.pass_test()
