#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Test checking that IntermediateDirectory can be defined in terms of
OutputDirectory. We previously had emitted the definition of
IntermediateDirectory before the definition of OutputDirectory.
This is required so that $(IntDir) can be based on $(OutDir).
"""

import TestGyp
import os

# NOTE: This test really is vcbuild/msbuild specific (not applicable to windows
#       ninja), as it is testing the msvs output location when opening an .sln
#       other than all.sln.
test = TestGyp.TestGyp(workdir='workarea_shared_output', formats=['msvs'])

test.run_gyp('hello.gyp')
test.set_configuration('Baz')

test.build('there/there.gyp', test.ALL)
test.must_exist(os.path.join(test.workdir, 'foo', 'there.exe'))
test.must_exist(os.path.join(test.workdir, 'foo', 'bar', 'there.obj'))

test.build('hello.gyp', test.ALL)
test.must_exist(os.path.join(test.workdir, 'foo', 'hello.exe'))
test.must_exist(os.path.join(test.workdir, 'foo', 'bar', 'hello.obj'))

if test.format == 'msvs':
  if test.uses_msbuild:
    test.must_contain('pull_in_there.vcxproj',
      '<IntDir>$(OutDir)bar\\</IntDir>')
  else:
    test.must_contain('pull_in_there.vcproj',
      'IntermediateDirectory="$(OutDir)bar\\"')

test.pass_test()
