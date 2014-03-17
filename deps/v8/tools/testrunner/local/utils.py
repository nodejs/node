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


import os
from os.path import exists
from os.path import isdir
from os.path import join
import platform
import re


def GetSuitePaths(test_root):
  def IsSuite(path):
    return isdir(path) and exists(join(path, 'testcfg.py'))
  return [ f for f in os.listdir(test_root) if IsSuite(join(test_root, f)) ]


# Reads a file into an array of strings
def ReadLinesFrom(name):
  lines = []
  with open(name) as f:
    for line in f:
      if line.startswith('#'): continue
      if '#' in line:
        line = line[:line.find('#')]
      line = line.strip()
      if not line: continue
      lines.append(line)
  return lines


def GuessOS():
  system = platform.system()
  if system == 'Linux':
    return 'linux'
  elif system == 'Darwin':
    return 'macos'
  elif system.find('CYGWIN') >= 0:
    return 'cygwin'
  elif system == 'Windows' or system == 'Microsoft':
    # On Windows Vista platform.system() can return 'Microsoft' with some
    # versions of Python, see http://bugs.python.org/issue1082
    return 'windows'
  elif system == 'FreeBSD':
    return 'freebsd'
  elif system == 'OpenBSD':
    return 'openbsd'
  elif system == 'SunOS':
    return 'solaris'
  elif system == 'NetBSD':
    return 'netbsd'
  else:
    return None


def UseSimulator(arch):
  machine = platform.machine()
  return (machine and
      (arch == "mipsel" or arch == "arm") and
      not arch.startswith(machine))


# This will default to building the 32 bit VM even on machines that are
# capable of running the 64 bit VM.
def DefaultArch():
  machine = platform.machine()
  machine = machine.lower()  # Windows 7 capitalizes 'AMD64'.
  if machine.startswith('arm'):
    return 'arm'
  elif (not machine) or (not re.match('(x|i[3-6])86$', machine) is None):
    return 'ia32'
  elif machine == 'i86pc':
    return 'ia32'
  elif machine == 'x86_64':
    return 'ia32'
  elif machine == 'amd64':
    return 'ia32'
  else:
    return None


def GuessWordsize():
  if '64' in platform.machine():
    return '64'
  else:
    return '32'


def IsWindows():
  return GuessOS() == 'windows'
