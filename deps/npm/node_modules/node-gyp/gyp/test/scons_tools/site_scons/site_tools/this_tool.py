# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# SCons "tool" module that simply sets a -D value.
def generate(env):
  env['CPPDEFINES'] = ['THIS_TOOL']

def exists(env):
  pass
