#!/usr/bin/env python
# Copyright 2019 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys

from testrunner.local import testsuite


class TestSuite(testsuite.TestSuite):
  pass

def GetSuite(*args, **kwargs):
  return TestSuite(*args, **kwargs)
