# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Custom gyp generator that doesn't do much."""

import gyp.common

generator_default_variables = {}

def GenerateOutput(target_list, target_dicts, data, params):
  f = open("MyBuildFile", "wb")
  f.write("Testing...\n")
  f.close()
