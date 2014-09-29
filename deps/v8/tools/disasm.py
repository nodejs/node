#!/usr/bin/env python
#
# Copyright 2011 the V8 project authors. All rights reserved.
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
import re
import subprocess
import tempfile


# Avoid using the slow (google-specific) wrapper around objdump.
OBJDUMP_BIN = "/usr/bin/objdump"
if not os.path.exists(OBJDUMP_BIN):
  OBJDUMP_BIN = "objdump"


_COMMON_DISASM_OPTIONS = ["-M", "intel-mnemonic", "-C"]

_DISASM_HEADER_RE = re.compile(r"[a-f0-9]+\s+<.*:$")
_DISASM_LINE_RE = re.compile(r"\s*([a-f0-9]+):\s*(\S.*)")

# Keys must match constants in Logger::LogCodeInfo.
_ARCH_MAP = {
  "ia32": "-m i386",
  "x64": "-m i386 -M x86-64",
  "arm": "-m arm",  # Not supported by our objdump build.
  "mips": "-m mips",  # Not supported by our objdump build.
  "arm64": "-m aarch64"
}


def GetDisasmLines(filename, offset, size, arch, inplace, arch_flags=""):
  tmp_name = None
  if not inplace:
    # Create a temporary file containing a copy of the code.
    assert arch in _ARCH_MAP, "Unsupported architecture '%s'" % arch
    arch_flags = arch_flags + " " +  _ARCH_MAP[arch]
    tmp_name = tempfile.mktemp(".v8code")
    command = "dd if=%s of=%s bs=1 count=%d skip=%d && " \
              "%s %s -D -b binary %s %s" % (
      filename, tmp_name, size, offset,
      OBJDUMP_BIN, ' '.join(_COMMON_DISASM_OPTIONS), arch_flags,
      tmp_name)
  else:
    command = "%s %s %s --start-address=%d --stop-address=%d -d %s " % (
      OBJDUMP_BIN, ' '.join(_COMMON_DISASM_OPTIONS), arch_flags,
      offset,
      offset + size,
      filename)
  process = subprocess.Popen(command,
                             shell=True,
                             stdout=subprocess.PIPE,
                             stderr=subprocess.STDOUT)
  out, err = process.communicate()
  lines = out.split("\n")
  header_line = 0
  for i, line in enumerate(lines):
    if _DISASM_HEADER_RE.match(line):
      header_line = i
      break
  if tmp_name:
    os.unlink(tmp_name)
  split_lines = []
  for line in lines[header_line + 1:]:
    match = _DISASM_LINE_RE.match(line)
    if match:
      line_address = int(match.group(1), 16)
      split_lines.append((line_address, match.group(2)))
  return split_lines
