#!/usr/bin/env python

# Copyright (c) 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that extra filters are pruned correctly for Visual Studio 2008.
"""

import re
import TestGyp


def strip_ws(str):
    return re.sub('^ +', '', str, flags=re.M).replace('\n', '')


test = TestGyp.TestGyp(formats=['msvs'])

test.run_gyp('filters.gyp', '-G', 'standalone', '-G', 'msvs_version=2008')

test.must_contain('no_source_files.vcproj', '<Files/>')

test.must_contain('one_source_file.vcproj', strip_ws('''\
<Files>
  <File RelativePath="..\\folder\\a.c"/>
</Files>
'''))

test.must_contain('two_source_files.vcproj', strip_ws('''\
<Files>
  <File RelativePath="..\\folder\\a.c"/>
  <File RelativePath="..\\folder\\b.c"/>
</Files>
'''))

test.must_contain('three_files_in_two_folders.vcproj', strip_ws('''\
<Files>
  <Filter Name="folder1">
    <File RelativePath="..\\folder1\\a.c"/>
    <File RelativePath="..\\folder1\\b.c"/>
  </Filter>
  <Filter Name="folder2">
    <File RelativePath="..\\folder2\\c.c"/>
  </Filter>
</Files>
'''))

test.must_contain('nested_folders.vcproj', strip_ws('''\
<Files>
  <Filter Name="folder1">
    <Filter Name="nested">
      <File RelativePath="..\\folder1\\nested\\a.c"/>
      <File RelativePath="..\\folder1\\nested\\b.c"/>
    </Filter>
    <Filter Name="other">
      <File RelativePath="..\\folder1\\other\\c.c"/>
    </Filter>
  </Filter>
  <Filter Name="folder2">
    <File RelativePath="..\\folder2\\d.c"/>
  </Filter>
</Files>
'''))


test.pass_test()
