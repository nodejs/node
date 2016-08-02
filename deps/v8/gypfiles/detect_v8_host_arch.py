#!/usr/bin/env python
# Copyright 2014 the V8 project authors. All rights reserved.
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

"""Outputs host CPU architecture in format recognized by gyp."""

import platform
import re
import sys


def main():
  print DoMain([])
  return 0

def DoMain(_):
  """Hook to be called from gyp without starting a separate python
  interpreter."""
  host_arch = platform.machine()
  host_system = platform.system();

  # Convert machine type to format recognized by gyp.
  if re.match(r'i.86', host_arch) or host_arch == 'i86pc':
    host_arch = 'ia32'
  elif host_arch in ['x86_64', 'amd64']:
    host_arch = 'x64'
  elif host_arch.startswith('arm'):
    host_arch = 'arm'
  elif host_arch == 'aarch64':
    host_arch = 'arm64'
  elif host_arch == 'mips64':
    host_arch = 'mips64el'
  elif host_arch.startswith('mips'):
    host_arch = 'mipsel'

  # Under AIX the value returned by platform.machine is not
  # the best indicator of the host architecture
  # AIX 6.1 which is the lowest level supported only provides
  # a 64 bit kernel
  if host_system == 'AIX':
    host_arch = 'ppc64'

  # platform.machine is based on running kernel. It's possible to use 64-bit
  # kernel with 32-bit userland, e.g. to give linker slightly more memory.
  # Distinguish between different userland bitness by querying
  # the python binary.
  if host_arch == 'x64' and platform.architecture()[0] == '32bit':
    host_arch = 'ia32'

  return host_arch

if __name__ == '__main__':
  sys.exit(main())
