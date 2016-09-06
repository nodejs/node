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
  print 'Activating MSVS 2013 again.'
  print 'Clobber after ICU roll.'
  print 'Moar clobbering...'
  print 'Remove build/android.gypi'
  print 'Cleanup after windows ninja switch attempt.'
  print 'Switching to pinned msvs toolchain.'
  print 'Clobbering to hopefully resolve problem with mksnapshot'
  print 'Clobber after ICU roll.'
  print 'Clobber after Android NDK update.'
  return 0


if __name__ == '__main__':
  sys.exit(main())
