#!/usr/bin/env python

# Copyright (c) 2011 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that msvs_prebuild and msvs_postbuild can be specified in both
VS 2008 and 2010.
"""

import TestGyp

test = TestGyp.TestGyp(formats=['msvs'], workdir='workarea_all')

test.run_gyp('buildevents.gyp', '-G', 'msvs_version=2008')
test.must_contain('main.vcproj', 'Name="VCPreBuildEventTool"')
test.must_contain('main.vcproj', 'Name="VCPostBuildEventTool"')

test.run_gyp('buildevents.gyp', '-G', 'msvs_version=2010')
test.must_contain('main.vcxproj', '<PreBuildEvent>')
test.must_contain('main.vcxproj', '<PostBuildEvent>')

test.pass_test()
