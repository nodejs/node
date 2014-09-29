#!/usr/bin/env python
# Copyright 2014 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
This file emits the list of reasons why a particular build needs to be clobbered
(or a list of 'landmines').
"""

import sys


def main():
  """
  ALL LANDMINES ARE EMITTED FROM HERE.
  """
  print 'Need to clobber after ICU52 roll.'
  print 'Landmines test.'
  print 'Activating MSVS 2013.'
  print 'Revert activation of MSVS 2013.'
  return 0


if __name__ == '__main__':
  sys.exit(main())
