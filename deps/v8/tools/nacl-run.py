#!/usr/bin/env python
#
# Copyright 2013 the V8 project authors. All rights reserved.
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

# This script executes the passed command line using the Native Client
# 'sel_ldr' container. It is derived from android-run.py.

import os
from os.path import join, dirname, abspath
import re
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
  output = file(outname).read()
  errors = file(errname).read()
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

def GetNaClArchFromNexe(nexe):
  try:
    p = subprocess.Popen(['file', nexe], stdout=subprocess.PIPE)
    out, err = p.communicate()
    lines = [re.sub("\s+", " " , line) for line in out.split('\n')]
    if lines[0].find(": ELF 32-bit LSB executable, Intel 80386") > 0:
      return "x86_32"
    if lines[0].find(": ELF 64-bit LSB executable, x86-64") > 0:
      return "x86_64"
  except:
    print 'file ' + sys.argv[1] + ' failed'
  return None

def GetNaClResources(nexe):
  nacl_sdk_dir = os.environ["NACL_SDK_ROOT"]
  nacl_arch = GetNaClArchFromNexe(nexe)
  if sys.platform.startswith("linux"):
    platform = "linux"
  elif sys.platform == "darwin":
    platform = "mac"
  else:
    print("NaCl V8 testing is supported on Linux and MacOS only.")
    sys.exit(1)

  if nacl_arch is "x86_64":
    toolchain = platform + "_x86_glibc"
    sel_ldr = "sel_ldr_x86_64"
    irt = "irt_core_x86_64.nexe"
    libdir = "lib64"
  elif nacl_arch is "x86_32":
    toolchain = platform + "_x86_glibc"
    sel_ldr = "sel_ldr_x86_32"
    irt = "irt_core_x86_32.nexe"
    libdir = "lib32"
  elif nacl_arch is "arm":
    print("NaCl V8 ARM support is not ready yet.")
    sys.exit(1)
  else:
    print("Invalid nexe %s with NaCl arch %s" % (nexe, nacl_arch))
    sys.exit(1)

  nacl_sel_ldr = os.path.join(nacl_sdk_dir, "tools", sel_ldr)
  nacl_irt = os.path.join(nacl_sdk_dir, "tools", irt)

  return (nacl_sdk_dir, nacl_sel_ldr, nacl_irt)

def Main():
  if (len(sys.argv) == 1):
    print("Usage: %s <command-to-run-on-device>" % sys.argv[0])
    return 1

  args = [Escape(arg) for arg in sys.argv[1:]]

  (nacl_sdk_dir, nacl_sel_ldr, nacl_irt) = GetNaClResources(sys.argv[1])

  # sel_ldr Options:
  # -c -c: disable validation (for performance)
  # -a: allow file access
  # -B <irt>: load the IRT
  command = ' '.join([nacl_sel_ldr, '-c', '-c', '-a', '-B', nacl_irt, '--'] +
                     args)
  error_code = Execute(command)
  return error_code

if __name__ == '__main__':
  sys.exit(Main())
