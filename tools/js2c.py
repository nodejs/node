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

# This is a utility for converting JavaScript source code into C-style
# char arrays. It is used for embedded JavaScript code in the V8
# library.

import os
import re
import sys
import string


def ToCArray(filename, lines):
  return ','.join(str(ord(c)) for c in lines)


def ReadFile(filename):
  file = open(filename, "rt")
  try:
    lines = file.read()
  finally:
    file.close()
  return lines


def ReadLines(filename):
  result = []
  for line in open(filename, "rt"):
    if '#' in line:
      line = line[:line.index('#')]
    line = line.strip()
    if len(line) > 0:
      result.append(line)
  return result


def LoadConfigFrom(name):
  import ConfigParser
  config = ConfigParser.ConfigParser()
  config.read(name)
  return config


def ParseValue(string):
  string = string.strip()
  if string.startswith('[') and string.endswith(']'):
    return string.lstrip('[').rstrip(']').split()
  else:
    return string


def ExpandConstants(lines, constants):
  for key, value in constants.items():
    lines = lines.replace(key, str(value))
  return lines


def ExpandMacros(lines, macros):
  for name, macro in macros.items():
    start = lines.find(name + '(', 0)
    while start != -1:
      # Scan over the arguments
      assert lines[start + len(name)] == '('
      height = 1
      end = start + len(name) + 1
      last_match = end
      arg_index = 0
      mapping = { }
      def add_arg(str):
        # Remember to expand recursively in the arguments
        replacement = ExpandMacros(str.strip(), macros)
        mapping[macro.args[arg_index]] = replacement
      while end < len(lines) and height > 0:
        # We don't count commas at higher nesting levels.
        if lines[end] == ',' and height == 1:
          add_arg(lines[last_match:end])
          last_match = end + 1
        elif lines[end] in ['(', '{', '[']:
          height = height + 1
        elif lines[end] in [')', '}', ']']:
          height = height - 1
        end = end + 1
      # Remember to add the last match.
      add_arg(lines[last_match:end-1])
      result = macro.expand(mapping)
      # Replace the occurrence of the macro with the expansion
      lines = lines[:start] + result + lines[end:]
      start = lines.find(name + '(', start)
  return lines


class TextMacro:
  def __init__(self, args, body):
    self.args = args
    self.body = body
  def expand(self, mapping):
    result = self.body
    for key, value in mapping.items():
        result = result.replace(key, value)
    return result

class PythonMacro:
  def __init__(self, args, fun):
    self.args = args
    self.fun = fun
  def expand(self, mapping):
    args = []
    for arg in self.args:
      args.append(mapping[arg])
    return str(self.fun(*args))

CONST_PATTERN = re.compile('^const\s+([a-zA-Z0-9_]+)\s*=\s*([^;]*);$')
MACRO_PATTERN = re.compile('^macro\s+([a-zA-Z0-9_]+)\s*\(([^)]*)\)\s*=\s*([^;]*);$')
PYTHON_MACRO_PATTERN = re.compile('^python\s+macro\s+([a-zA-Z0-9_]+)\s*\(([^)]*)\)\s*=\s*([^;]*);$')

def ReadMacros(lines):
  constants = { }
  macros = { }
  for line in lines:
    hash = line.find('#')
    if hash != -1: line = line[:hash]
    line = line.strip()
    if len(line) is 0: continue
    const_match = CONST_PATTERN.match(line)
    if const_match:
      name = const_match.group(1)
      value = const_match.group(2).strip()
      constants[name] = value
    else:
      macro_match = MACRO_PATTERN.match(line)
      if macro_match:
        name = macro_match.group(1)
        args = map(string.strip, macro_match.group(2).split(','))
        body = macro_match.group(3).strip()
        macros[name] = TextMacro(args, body)
      else:
        python_match = PYTHON_MACRO_PATTERN.match(line)
        if python_match:
          name = python_match.group(1)
          args = map(string.strip, python_match.group(2).split(','))
          body = python_match.group(3).strip()
          fun = eval("lambda " + ",".join(args) + ': ' + body)
          macros[name] = PythonMacro(args, fun)
        else:
          raise Exception("Illegal line: " + line)
  return (constants, macros)


HEADER_TEMPLATE = """\
#ifndef node_natives_h
#define node_natives_h
namespace node {

%(source_lines)s\

struct _native {
  const char* name;
  const unsigned char* source;
  size_t source_len;
};

static const struct _native natives[] = { %(native_lines)s };

}
#endif
"""


NATIVE_DECLARATION = """\
  { "%(id)s", %(escaped_id)s_native, sizeof(%(escaped_id)s_native) },
"""

SOURCE_DECLARATION = """\
  const unsigned char %(escaped_id)s_native[] = { %(data)s };
"""


GET_DELAY_INDEX_CASE = """\
    if (strcmp(name, "%(id)s") == 0) return %(i)i;
"""


GET_DELAY_SCRIPT_SOURCE_CASE = """\
    if (index == %(i)i) return Vector<const char>(%(id)s, %(length)i);
"""


GET_DELAY_SCRIPT_NAME_CASE = """\
    if (index == %(i)i) return Vector<const char>("%(name)s", %(length)i);
"""

def JS2C(source, target):
  ids = []
  delay_ids = []
  modules = []
  # Locate the macros file name.
  consts = {}
  macros = {}
  macro_lines = []

  for s in source:
    if (os.path.split(str(s))[1]).endswith('macros.py'):
      macro_lines.extend(ReadLines(str(s)))
    else:
      modules.append(s)

  # Process input from all *macro.py files
  (consts, macros) = ReadMacros(macro_lines)

  # Build source code lines
  source_lines = [ ]
  source_lines_empty = []

  native_lines = []

  for s in modules:
    delay = str(s).endswith('-delay.js')
    lines = ReadFile(str(s))

    lines = ExpandConstants(lines, consts)
    lines = ExpandMacros(lines, macros)
    data = ToCArray(s, lines)

    # On Windows, "./foo.bar" in the .gyp file is passed as "foo.bar"
    # so don't assume there is always a slash in the file path.
    if '/' in s or '\\' in s:
      id = '/'.join(re.split('/|\\\\', s)[1:])
    else:
      id = s

    if '.' in id:
      id = id.split('.', 1)[0]

    if delay: id = id[:-6]
    if delay:
      delay_ids.append((id, len(lines)))
    else:
      ids.append((id, len(lines)))

    escaped_id = id.replace('-', '_').replace('/', '_')
    source_lines.append(SOURCE_DECLARATION % {
      'id': id,
      'escaped_id': escaped_id,
      'data': data
    })
    source_lines_empty.append(SOURCE_DECLARATION % {
      'id': id,
      'escaped_id': escaped_id,
      'data': 0
    })
    native_lines.append(NATIVE_DECLARATION % {
      'id': id,
      'escaped_id': escaped_id
    })

  # Build delay support functions
  get_index_cases = [ ]
  get_script_source_cases = [ ]
  get_script_name_cases = [ ]

  i = 0
  for (id, length) in delay_ids:
    native_name = "native %s.js" % id
    get_index_cases.append(GET_DELAY_INDEX_CASE % { 'id': id, 'i': i })
    get_script_source_cases.append(GET_DELAY_SCRIPT_SOURCE_CASE % {
      'id': id,
      'length': length,
      'i': i
    })
    get_script_name_cases.append(GET_DELAY_SCRIPT_NAME_CASE % {
      'name': native_name,
      'length': len(native_name),
      'i': i
    });
    i = i + 1

  for (id, length) in ids:
    native_name = "native %s.js" % id
    get_index_cases.append(GET_DELAY_INDEX_CASE % { 'id': id, 'i': i })
    get_script_source_cases.append(GET_DELAY_SCRIPT_SOURCE_CASE % {
      'id': id,
      'length': length,
      'i': i
    })
    get_script_name_cases.append(GET_DELAY_SCRIPT_NAME_CASE % {
      'name': native_name,
      'length': len(native_name),
      'i': i
    });
    i = i + 1

  # Emit result
  output = open(str(target[0]), "w")
  output.write(HEADER_TEMPLATE % {
    'builtin_count': len(ids) + len(delay_ids),
    'delay_count': len(delay_ids),
    'source_lines': "\n".join(source_lines),
    'native_lines': "\n".join(native_lines),
    'get_index_cases': "".join(get_index_cases),
    'get_script_source_cases': "".join(get_script_source_cases),
    'get_script_name_cases': "".join(get_script_name_cases)
  })
  output.close()

  if len(target) > 1:
    output = open(str(target[1]), "w")
    output.write(HEADER_TEMPLATE % {
      'builtin_count': len(ids) + len(delay_ids),
      'delay_count': len(delay_ids),
      'source_lines': "\n".join(source_lines_empty),
      'get_index_cases': "".join(get_index_cases),
      'get_script_source_cases': "".join(get_script_source_cases),
      'get_script_name_cases': "".join(get_script_name_cases)
    })
    output.close()

def main():
  natives = sys.argv[1]
  source_files = sys.argv[2:]
  JS2C(source_files, [natives])

if __name__ == "__main__":
  main()
