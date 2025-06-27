#!/usr/bin/env python3
#
# Copyright 2017 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


from __future__ import absolute_import
import sys

from testrunner import standard_runner


if __name__ == "__main__":
  sys.exit(standard_runner.StandardTestRunner().execute())
