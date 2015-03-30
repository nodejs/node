#!/usr/bin/env python

# Copyright (c) 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that extra filters are pruned correctly for Visual Studio 2010
and later.
"""

import TestGyp


test = TestGyp.TestGyp(formats=['msvs'])

test.run_gyp('filters.gyp', '-G', 'standalone', '-G', 'msvs_version=2010')

test.must_not_exist('no_source_files.vcxproj.filters')

test.must_not_exist('one_source_file.vcxproj.filters')

test.must_not_exist('two_source_files.vcxproj.filters')

test.must_contain('three_files_in_two_folders.vcxproj.filters', '''\
  <ItemGroup>
    <ClCompile Include="..\\folder1\\a.c">
      <Filter>folder1</Filter>
    </ClCompile>
    <ClCompile Include="..\\folder1\\b.c">
      <Filter>folder1</Filter>
    </ClCompile>
    <ClCompile Include="..\\folder2\\c.c">
      <Filter>folder2</Filter>
    </ClCompile>
  </ItemGroup>
'''.replace('\n', '\r\n'))

test.must_contain('nested_folders.vcxproj.filters', '''\
  <ItemGroup>
    <ClCompile Include="..\\folder1\\nested\\a.c">
      <Filter>folder1\\nested</Filter>
    </ClCompile>
    <ClCompile Include="..\\folder2\\d.c">
      <Filter>folder2</Filter>
    </ClCompile>
    <ClCompile Include="..\\folder1\\nested\\b.c">
      <Filter>folder1\\nested</Filter>
    </ClCompile>
    <ClCompile Include="..\\folder1\\other\\c.c">
      <Filter>folder1\\other</Filter>
    </ClCompile>
  </ItemGroup>
'''.replace('\n', '\r\n'))


test.pass_test()
