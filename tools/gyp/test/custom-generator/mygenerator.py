# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Custom gyp generator that doesn't do much."""

generator_default_variables = {}


# noinspection PyUnusedLocal
def GenerateOutput(target_list, target_dicts, data, params):
  f = open("MyBuildFile", "w")
  f.write("Testing...\n")
  f.close()
