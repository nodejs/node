#!/usr/bin/env python
#
# Copyright 2006-2008 the V8 project authors. All rights reserved.
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

"""
This is a utility for converting JavaScript source code into uint16_t[],
that are used for embedding JavaScript code into the Node.js binary.
"""
import argparse
import os
import re
import functools
import codecs

def ReadFile(filename):
  if is_verbose:
    print(filename)
  with codecs.open(filename, "r", "utf-8") as f:
    lines = f.read()
    return lines


TEMPLATE = """
#include "env-inl.h"
#include "node_native_module.h"
#include "node_internals.h"

namespace node {{

namespace native_module {{

{0}

void NativeModuleLoader::LoadJavaScriptSource() {{
  {1}
}}

UnionBytes NativeModuleLoader::GetConfig() {{
  return UnionBytes(config_raw, {2});  // config.gypi
}}

}}  // namespace native_module

}}  // namespace node
"""

ONE_BYTE_STRING = """
static const uint8_t {0}[] = {{
{1}
}};
"""

TWO_BYTE_STRING = """
static const uint16_t {0}[] = {{
{1}
}};
"""

INITIALIZER = 'source_.emplace("{0}", UnionBytes{{{1}, {2}}});'

CONFIG_GYPI_ID = 'config_raw'

SLUGGER_RE =re.compile('[.\-/]')

is_verbose = False

def GetDefinition(var, source, step=30):
  template = ONE_BYTE_STRING
  code_points = [ord(c) for c in source]
  if any(c > 127 for c in code_points):
    template = TWO_BYTE_STRING
    # Treat non-ASCII as UTF-8 and encode as UTF-16 Little Endian.
    encoded_source = bytearray(source, 'utf-16le')
    code_points = [
      encoded_source[i] + (encoded_source[i + 1] * 256)
      for i in range(0, len(encoded_source), 2)
    ]

  # For easier debugging, align to the common 3 char for code-points.
  elements_s = ['%3s' % x for x in code_points]
  # Put no more then `step` code-points in a line.
  slices = [elements_s[i:i + step] for i in range(0, len(elements_s), step)]
  lines = [','.join(s) for s in slices]
  array_content = ',\n'.join(lines)
  definition = template.format(var, array_content)

  return definition, len(code_points)


def AddModule(filename, definitions, initializers):
  code = ReadFile(filename)
  name = NormalizeFileName(filename)
  slug = SLUGGER_RE.sub('_', name)
  var = slug + '_raw'
  definition, size = GetDefinition(var, code)
  initializer = INITIALIZER.format(name, var, size)
  definitions.append(definition)
  initializers.append(initializer)

def NormalizeFileName(filename):
  split = filename.split(os.path.sep)
  if split[0] == 'deps':
    split = ['internal'] + split
  else:  # `lib/**/*.js` so drop the 'lib' part
    split = split[1:]
  if len(split):
    filename = '/'.join(split)
  return os.path.splitext(filename)[0]


def JS2C(source_files, target):
  # Build source code lines
  definitions = []
  initializers = []

  for filename in source_files['.js']:
    AddModule(filename, definitions, initializers)

  config_def, config_size = handle_config_gypi(source_files['config.gypi'])
  definitions.append(config_def)

  # Emit result
  definitions = ''.join(definitions)
  initializers = '\n  '.join(initializers)
  out = TEMPLATE.format(definitions, initializers, config_size)
  write_if_chaged(out, target)


def handle_config_gypi(config_filename):
  # if its a gypi file we're going to want it as json
  # later on anyway, so get it out of the way now
  config = ReadFile(config_filename)
  config = jsonify(config)
  config_def, config_size = GetDefinition(CONFIG_GYPI_ID, config)
  return config_def, config_size


def jsonify(config):
  # 1. string comments
  config = re.sub(r'#.*?\n', '', config)
  # 3. normalize string literals from ' into "
  config = re.sub('\'', '"', config)
  # 2. turn pseudo-booleans strings into Booleans
  config = re.sub('"true"', 'true', config)
  config = re.sub('"false"', 'false', config)
  return config


def write_if_chaged(content, target):
  if os.path.exists(target):
    with open(target, 'rt') as existing:
      old_content = existing.read()
  else:
    old_content = ''
  if old_content == content:
    return
  with open(target, "wt") as output:
    output.write(content)


def SourceFileByExt(files_by_ext, filename):
  """
  :type files_by_ext: dict
  :type filename: str
  :rtype: dict
  """
  ext = os.path.splitext(filename)[-1]
  files_by_ext.setdefault(ext, []).append(filename)
  return files_by_ext

def main():
  parser = argparse.ArgumentParser(
    description='Convert code files into `uint16_t[]`s',
    fromfile_prefix_chars='@'
  )
  parser.add_argument('--target', help='output file')
  parser.add_argument('--verbose', action='store_true', help='output file')
  parser.add_argument('sources', nargs='*', help='input files')
  options = parser.parse_args()
  global is_verbose
  is_verbose = options.verbose
  source_files = functools.reduce(SourceFileByExt, options.sources, {})
  # Should have exactly 2 types: `.js`, and `.gypi`
  assert len(source_files) == 2
  # Currently config.gypi is the only `.gypi` file allowed
  assert source_files['.gypi'] == ['config.gypi']
  source_files['config.gypi'] = source_files.pop('.gypi')[0]
  JS2C(source_files, options.target)


if __name__ == "__main__":
  main()
