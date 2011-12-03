#!/usr/bin/env python
# Copyright (c) 2011 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""These functions are executed via gyp-sun-tool when using the Makefile
generator."""

import fcntl
import os
import struct
import subprocess
import sys


def main(args):
  executor = SunTool()
  executor.Dispatch(args)


class SunTool(object):
  """This class performs all the SunOS tooling steps. The methods can either be
  executed directly, or dispatched from an argument list."""

  def Dispatch(self, args):
    """Dispatches a string command to a method."""
    if len(args) < 1:
      raise Exception("Not enough arguments")

    method = "Exec%s" % self._CommandifyName(args[0])
    getattr(self, method)(*args[1:])

  def _CommandifyName(self, name_string):
    """Transforms a tool name like copy-info-plist to CopyInfoPlist"""
    return name_string.title().replace('-', '')

  def ExecFlock(self, lockfile, *cmd_list):
    """Emulates the most basic behavior of Linux's flock(1)."""
    # Rely on exception handling to report errors.
    # Note that the stock python on SunOS has a bug
    # where fcntl.flock(fd, LOCK_EX) always fails
    # with EBADF, that's why we use this F_SETLK
    # hack instead.
    fd = os.open(lockfile, os.O_WRONLY|os.O_NOCTTY|os.O_CREAT, 0666)
    op = struct.pack('hhllhhl', fcntl.F_WRLCK, 0, 0, 0, 0, 0, 0)
    fcntl.fcntl(fd, fcntl.F_SETLK, op)
    return subprocess.call(cmd_list)


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
