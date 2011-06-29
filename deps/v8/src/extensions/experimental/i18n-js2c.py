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

# This is a utility for converting I18N JavaScript source code into C-style
# char arrays. It is used for embedded JavaScript code in the V8
# library.
# This is a pared down copy of v8/tools/js2c.py that avoids use of
# v8/src/natives.h and produces different cc template.

import os, re, sys, string


def ToCArray(lines):
  result = []
  for chr in lines:
    value = ord(chr)
    assert value < 128
    result.append(str(value))
  result.append("0")
  return ", ".join(result)


def RemoveCommentsAndTrailingWhitespace(lines):
  lines = re.sub(r'//.*\n', '\n', lines) # end-of-line comments
  lines = re.sub(re.compile(r'/\*.*?\*/', re.DOTALL), '', lines) # comments.
  lines = re.sub(r'\s+\n+', '\n', lines) # trailing whitespace
  return lines


def ReadFile(filename):
  file = open(filename, "rt")
  try:
    lines = file.read()
  finally:
    file.close()
  return lines


EVAL_PATTERN = re.compile(r'\beval\s*\(');
WITH_PATTERN = re.compile(r'\bwith\s*\(');


def Validate(lines, file):
  lines = RemoveCommentsAndTrailingWhitespace(lines)
  # Because of simplified context setup, eval and with is not
  # allowed in the natives files.
  eval_match = EVAL_PATTERN.search(lines)
  if eval_match:
    raise ("Eval disallowed in natives: %s" % file)
  with_match = WITH_PATTERN.search(lines)
  if with_match:
    raise ("With statements disallowed in natives: %s" % file)


HEADER_TEMPLATE = """\
// Copyright 2011 Google Inc. All Rights Reserved.

// This file was generated from .js source files by gyp.  If you
// want to make changes to this file you should either change the
// javascript source files or the i18n-js2c.py script.

#include "src/extensions/experimental/i18n-natives.h"

namespace v8 {
namespace internal {

// static
const char* I18Natives::GetScriptSource() {
  // JavaScript source gets injected here.
  static const char i18n_source[] = {%s};

  return i18n_source;
}

}  // internal
}  // v8
"""


def JS2C(source, target):
  filename = str(source)

  lines = ReadFile(filename)
  Validate(lines, filename)
  data = ToCArray(lines)

  # Emit result
  output = open(target, "w")
  output.write(HEADER_TEMPLATE % data)
  output.close()


def main():
  target = sys.argv[1]
  source = sys.argv[2]
  JS2C(source, target)


if __name__ == "__main__":
  main()
