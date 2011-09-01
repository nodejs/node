#!/usr/bin/env python
# Copyright (c) 2011 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""These functions are executed via gyp-sun-tool when using the Makefile generator."""

import os
import fcntl
import plistlib
import shutil
import string
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
    fd = os.open(lockfile, os.O_RDONLY|os.O_NOCTTY|os.O_CREAT, 0o666)
    fcntl.flock(fd, fcntl.LOCK_EX)
    return subprocess.call(cmd_list)
