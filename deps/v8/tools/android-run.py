#!/usr/bin/env python
#
# Copyright 2012 the V8 project authors. All rights reserved.
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
#       copyright notice, this list of conditions and the following
#       disclaimer in the documentation and/or other materials provided
#       with the distribution.
#     * Neither the name of Google Inc. nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# This script executes the passed command line on Android device
# using 'adb shell' command. Unfortunately, 'adb shell' always
# returns exit code 0, ignoring the exit code of executed command.
# Since we need to return non-zero exit code if the command failed,
# we augment the passed command line with exit code checking statement
# and output special error string in case of non-zero exit code.
# Then we parse the output of 'adb shell' and look for that error string.

# for py2/py3 compatibility
from __future__ import print_function

import os
from os.path import join, dirname, abspath
import subprocess
import sys
import tempfile

def Check(output, errors):
  failed = any([s.startswith('/system/bin/sh:') or s.startswith('ANDROID')
                for s in output.split('\n')])
  return 1 if failed else 0

def Execute(cmdline):
  (fd_out, outname) = tempfile.mkstemp()
  (fd_err, errname) = tempfile.mkstemp()
  process = subprocess.Popen(
    args=cmdline,
    shell=True,
    stdout=fd_out,
    stderr=fd_err,
  )
  exit_code = process.wait()
  os.close(fd_out)
  os.close(fd_err)
  output = open(outname).read()
  errors = open(errname).read()
  os.unlink(outname)
  os.unlink(errname)
  sys.stdout.write(output)
  sys.stderr.write(errors)
  return exit_code or Check(output, errors)

def Escape(arg):
  def ShouldEscape():
    for x in arg:
      if not x.isalnum() and x != '-' and x != '_':
        return True
    return False

  return arg if not ShouldEscape() else '"%s"' % (arg.replace('"', '\\"'))

def WriteToTemporaryFile(data):
  (fd, fname) = tempfile.mkstemp()
  os.close(fd)
  tmp_file = open(fname, "w")
  tmp_file.write(data)
  tmp_file.close()
  return fname

def Main():
  if (len(sys.argv) == 1):
    print("Usage: %s <command-to-run-on-device>" % sys.argv[0])
    return 1
  workspace = abspath(join(dirname(sys.argv[0]), '..'))
  v8_root = "/data/local/tmp/v8"
  android_workspace = os.getenv("ANDROID_V8", v8_root)
  args = [Escape(arg) for arg in sys.argv[1:]]
  script = (" ".join(args) + "\n"
            "case $? in\n"
            "  0) ;;\n"
            "  *) echo \"ANDROID: Error returned by test\";;\n"
            "esac\n")
  script = script.replace(workspace, android_workspace)
  script_file = WriteToTemporaryFile(script)
  android_script_file = android_workspace + "/" + script_file
  command =  ("adb push '%s' %s;" % (script_file, android_script_file) +
              "adb shell 'cd %s && sh %s';" % (v8_root, android_script_file) +
              "adb shell 'rm %s'" % android_script_file)
  error_code = Execute(command)
  os.unlink(script_file)
  return error_code

if __name__ == '__main__':
  sys.exit(Main())
