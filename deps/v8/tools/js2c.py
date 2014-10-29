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

# This is a utility for converting JavaScript source code into C-style
# char arrays. It is used for embedded JavaScript code in the V8
# library.

import os, re, sys, string
import optparse
import jsmin
import bz2
import textwrap


class Error(Exception):
  def __init__(self, msg):
    Exception.__init__(self, msg)


def ToCArray(byte_sequence):
  result = []
  for chr in byte_sequence:
    result.append(str(ord(chr)))
  joined = ", ".join(result)
  return textwrap.fill(joined, 80)


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


EVAL_PATTERN = re.compile(r'\beval\s*\(')
WITH_PATTERN = re.compile(r'\bwith\s*\(')

def Validate(lines):
  # Because of simplified context setup, eval and with is not
  # allowed in the natives files.
  if EVAL_PATTERN.search(lines):
    raise Error("Eval disallowed in natives.")
  if WITH_PATTERN.search(lines):
    raise Error("With statements disallowed in natives.")

  # Pass lines through unchanged.
  return lines


def ExpandConstants(lines, constants):
  for key, value in constants:
    lines = key.sub(str(value), lines)
  return lines


def ExpandMacroDefinition(lines, pos, name_pattern, macro, expander):
  pattern_match = name_pattern.search(lines, pos)
  while pattern_match is not None:
    # Scan over the arguments
    height = 1
    start = pattern_match.start()
    end = pattern_match.end()
    assert lines[end - 1] == '('
    last_match = end
    arg_index = [0]  # Wrap state into array, to work around Python "scoping"
    mapping = { }
    def add_arg(str):
      # Remember to expand recursively in the arguments
      replacement = expander(str.strip())
      mapping[macro.args[arg_index[0]]] = replacement
      arg_index[0] += 1
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
    pattern_match = name_pattern.search(lines, start + len(result))
  return lines

def ExpandMacros(lines, macros):
  # We allow macros to depend on the previously declared macros, but
  # we don't allow self-dependecies or recursion.
  for name_pattern, macro in reversed(macros):
    def expander(s):
      return ExpandMacros(s, macros)
    lines = ExpandMacroDefinition(lines, 0, name_pattern, macro, expander)
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

CONST_PATTERN = re.compile(r'^const\s+([a-zA-Z0-9_]+)\s*=\s*([^;]*);$')
MACRO_PATTERN = re.compile(r'^macro\s+([a-zA-Z0-9_]+)\s*\(([^)]*)\)\s*=\s*([^;]*);$')
PYTHON_MACRO_PATTERN = re.compile(r'^python\s+macro\s+([a-zA-Z0-9_]+)\s*\(([^)]*)\)\s*=\s*([^;]*);$')


def ReadMacros(lines):
  constants = []
  macros = []
  for line in lines.split('\n'):
    hash = line.find('#')
    if hash != -1: line = line[:hash]
    line = line.strip()
    if len(line) is 0: continue
    const_match = CONST_PATTERN.match(line)
    if const_match:
      name = const_match.group(1)
      value = const_match.group(2).strip()
      constants.append((re.compile("\\b%s\\b" % name), value))
    else:
      macro_match = MACRO_PATTERN.match(line)
      if macro_match:
        name = macro_match.group(1)
        args = [match.strip() for match in macro_match.group(2).split(',')]
        body = macro_match.group(3).strip()
        macros.append((re.compile("\\b%s\\(" % name), TextMacro(args, body)))
      else:
        python_match = PYTHON_MACRO_PATTERN.match(line)
        if python_match:
          name = python_match.group(1)
          args = [match.strip() for match in python_match.group(2).split(',')]
          body = python_match.group(3).strip()
          fun = eval("lambda " + ",".join(args) + ': ' + body)
          macros.append((re.compile("\\b%s\\(" % name), PythonMacro(args, fun)))
        else:
          raise Error("Illegal line: " + line)
  return (constants, macros)

INLINE_MACRO_PATTERN = re.compile(r'macro\s+([a-zA-Z0-9_]+)\s*\(([^)]*)\)\s*\n')
INLINE_MACRO_END_PATTERN = re.compile(r'endmacro\s*\n')

def ExpandInlineMacros(lines):
  pos = 0
  while True:
    macro_match = INLINE_MACRO_PATTERN.search(lines, pos)
    if macro_match is None:
      # no more macros
      return lines
    name = macro_match.group(1)
    args = [match.strip() for match in macro_match.group(2).split(',')]
    end_macro_match = INLINE_MACRO_END_PATTERN.search(lines, macro_match.end());
    if end_macro_match is None:
      raise Error("Macro %s unclosed" % name)
    body = lines[macro_match.end():end_macro_match.start()]

    # remove macro definition
    lines = lines[:macro_match.start()] + lines[end_macro_match.end():]
    name_pattern = re.compile("\\b%s\\(" % name)
    macro = TextMacro(args, body)

    # advance position to where the macro defintion was
    pos = macro_match.start()

    def non_expander(s):
      return s
    lines = ExpandMacroDefinition(lines, pos, name_pattern, macro, non_expander)


INLINE_CONSTANT_PATTERN = re.compile(r'const\s+([a-zA-Z0-9_]+)\s*=\s*([^;\n]+)[;\n]')

def ExpandInlineConstants(lines):
  pos = 0
  while True:
    const_match = INLINE_CONSTANT_PATTERN.search(lines, pos)
    if const_match is None:
      # no more constants
      return lines
    name = const_match.group(1)
    replacement = const_match.group(2)
    name_pattern = re.compile("\\b%s\\b" % name)

    # remove constant definition and replace
    lines = (lines[:const_match.start()] +
             re.sub(name_pattern, replacement, lines[const_match.end():]))

    # advance position to where the constant defintion was
    pos = const_match.start()


HEADER_TEMPLATE = """\
// Copyright 2011 Google Inc. All Rights Reserved.

// This file was generated from .js source files by GYP.  If you
// want to make changes to this file you should either change the
// javascript source files or the GYP script.

#include "src/v8.h"
#include "src/natives.h"
#include "src/utils.h"

namespace v8 {
namespace internal {

%(sources_declaration)s\

%(raw_sources_declaration)s\

  template <>
  int NativesCollection<%(type)s>::GetBuiltinsCount() {
    return %(builtin_count)i;
  }

  template <>
  int NativesCollection<%(type)s>::GetDebuggerCount() {
    return %(debugger_count)i;
  }

  template <>
  int NativesCollection<%(type)s>::GetIndex(const char* name) {
%(get_index_cases)s\
    return -1;
  }

  template <>
  int NativesCollection<%(type)s>::GetRawScriptsSize() {
    return %(raw_total_length)i;
  }

  template <>
  Vector<const char> NativesCollection<%(type)s>::GetRawScriptSource(int index) {
%(get_raw_script_source_cases)s\
    return Vector<const char>("", 0);
  }

  template <>
  Vector<const char> NativesCollection<%(type)s>::GetScriptName(int index) {
%(get_script_name_cases)s\
    return Vector<const char>("", 0);
  }

  template <>
  Vector<const byte> NativesCollection<%(type)s>::GetScriptsSource() {
    return Vector<const byte>(sources, %(total_length)i);
  }

  template <>
  void NativesCollection<%(type)s>::SetRawScriptsSource(Vector<const char> raw_source) {
    DCHECK(%(raw_total_length)i == raw_source.length());
    raw_sources = raw_source.start();
  }

}  // internal
}  // v8
"""

SOURCES_DECLARATION = """\
  static const byte sources[] = { %s };
"""


RAW_SOURCES_COMPRESSION_DECLARATION = """\
  static const char* raw_sources = NULL;
"""


RAW_SOURCES_DECLARATION = """\
  static const char* raw_sources = reinterpret_cast<const char*>(sources);
"""


GET_INDEX_CASE = """\
    if (strcmp(name, "%(id)s") == 0) return %(i)i;
"""


GET_RAW_SCRIPT_SOURCE_CASE = """\
    if (index == %(i)i) return Vector<const char>(raw_sources + %(offset)i, %(raw_length)i);
"""


GET_SCRIPT_NAME_CASE = """\
    if (index == %(i)i) return Vector<const char>("%(name)s", %(length)i);
"""


def BuildFilterChain(macro_filename):
  """Build the chain of filter functions to be applied to the sources.

  Args:
    macro_filename: Name of the macro file, if any.

  Returns:
    A function (string -> string) that reads a source file and processes it.
  """
  filter_chain = [ReadFile]

  if macro_filename:
    (consts, macros) = ReadMacros(ReadFile(macro_filename))
    filter_chain.append(lambda l: ExpandConstants(l, consts))
    filter_chain.append(lambda l: ExpandMacros(l, macros))

  filter_chain.extend([
    RemoveCommentsAndTrailingWhitespace,
    ExpandInlineMacros,
    ExpandInlineConstants,
    Validate,
    jsmin.JavaScriptMinifier().JSMinify
  ])

  def chain(f1, f2):
    return lambda x: f2(f1(x))

  return reduce(chain, filter_chain)


class Sources:
  def __init__(self):
    self.names = []
    self.modules = []
    self.is_debugger_id = []


def IsDebuggerFile(filename):
  return filename.endswith("-debugger.js")

def IsMacroFile(filename):
  return filename.endswith("macros.py")


def PrepareSources(source_files):
  """Read, prepare and assemble the list of source files.

  Args:
    sources: List of Javascript-ish source files. A file named macros.py
        will be treated as a list of macros.

  Returns:
    An instance of Sources.
  """
  macro_file = None
  macro_files = filter(IsMacroFile, source_files)
  assert len(macro_files) in [0, 1]
  if macro_files:
    source_files.remove(macro_files[0])
    macro_file = macro_files[0]

  filters = BuildFilterChain(macro_file)

  # Sort 'debugger' sources first.
  source_files = sorted(source_files,
                        lambda l,r: IsDebuggerFile(r) - IsDebuggerFile(l))

  result = Sources()
  for source in source_files:
    try:
      lines = filters(source)
    except Error as e:
      raise Error("In file %s:\n%s" % (source, str(e)))

    result.modules.append(lines);

    is_debugger = IsDebuggerFile(source)
    result.is_debugger_id.append(is_debugger);

    name = os.path.basename(source)[:-3]
    result.names.append(name if not is_debugger else name[:-9]);
  return result


def BuildMetadata(sources, source_bytes, native_type):
  """Build the meta data required to generate a libaries file.

  Args:
    sources: A Sources instance with the prepared sources.
    source_bytes: A list of source bytes.
        (The concatenation of all sources; might be compressed.)
    native_type: The parameter for the NativesCollection template.

  Returns:
    A dictionary for use with HEADER_TEMPLATE.
  """
  total_length = len(source_bytes)
  raw_sources = "".join(sources.modules)

  # The sources are expected to be ASCII-only.
  assert not filter(lambda value: ord(value) >= 128, raw_sources)

  # Loop over modules and build up indices into the source blob:
  get_index_cases = []
  get_script_name_cases = []
  get_raw_script_source_cases = []
  offset = 0
  for i in xrange(len(sources.modules)):
    native_name = "native %s.js" % sources.names[i]
    d = {
        "i": i,
        "id": sources.names[i],
        "name": native_name,
        "length": len(native_name),
        "offset": offset,
        "raw_length": len(sources.modules[i]),
    }
    get_index_cases.append(GET_INDEX_CASE % d)
    get_script_name_cases.append(GET_SCRIPT_NAME_CASE % d)
    get_raw_script_source_cases.append(GET_RAW_SCRIPT_SOURCE_CASE % d)
    offset += len(sources.modules[i])
  assert offset == len(raw_sources)

  # If we have the raw sources we can declare them accordingly.
  have_raw_sources = source_bytes == raw_sources
  raw_sources_declaration = (RAW_SOURCES_DECLARATION
      if have_raw_sources else RAW_SOURCES_COMPRESSION_DECLARATION)

  metadata = {
    "builtin_count": len(sources.modules),
    "debugger_count": sum(sources.is_debugger_id),
    "sources_declaration": SOURCES_DECLARATION % ToCArray(source_bytes),
    "raw_sources_declaration": raw_sources_declaration,
    "raw_total_length": sum(map(len, sources.modules)),
    "total_length": total_length,
    "get_index_cases": "".join(get_index_cases),
    "get_raw_script_source_cases": "".join(get_raw_script_source_cases),
    "get_script_name_cases": "".join(get_script_name_cases),
    "type": native_type,
  }
  return metadata


def CompressMaybe(sources, compression_type):
  """Take the prepared sources and generate a sequence of bytes.

  Args:
    sources: A Sources instance with the prepared sourced.
    compression_type: string, describing the desired compression.

  Returns:
    A sequence of bytes.
  """
  sources_bytes = "".join(sources.modules)
  if compression_type == "off":
    return sources_bytes
  elif compression_type == "bz2":
    return bz2.compress(sources_bytes)
  else:
    raise Error("Unknown compression type %s." % compression_type)


def PutInt(blob_file, value):
  assert(value >= 0 and value < (1 << 20))
  size = 1 if (value < 1 << 6) else (2 if (value < 1 << 14) else 3)
  value_with_length = (value << 2) | size

  byte_sequence = bytearray()
  for i in xrange(size):
    byte_sequence.append(value_with_length & 255)
    value_with_length >>= 8;
  blob_file.write(byte_sequence)


def PutStr(blob_file, value):
  PutInt(blob_file, len(value));
  blob_file.write(value);


def WriteStartupBlob(sources, startup_blob):
  """Write a startup blob, as expected by V8 Initialize ...
    TODO(vogelheim): Add proper method name.

  Args:
    sources: A Sources instance with the prepared sources.
    startup_blob_file: Name of file to write the blob to.
  """
  output = open(startup_blob, "wb")

  debug_sources = sum(sources.is_debugger_id);
  PutInt(output, debug_sources)
  for i in xrange(debug_sources):
    PutStr(output, sources.names[i]);
    PutStr(output, sources.modules[i]);

  PutInt(output, len(sources.names) - debug_sources)
  for i in xrange(debug_sources, len(sources.names)):
    PutStr(output, sources.names[i]);
    PutStr(output, sources.modules[i]);

  output.close()


def JS2C(source, target, native_type, compression_type, raw_file, startup_blob):
  sources = PrepareSources(source)
  sources_bytes = CompressMaybe(sources, compression_type)
  metadata = BuildMetadata(sources, sources_bytes, native_type)

  # Optionally emit raw file.
  if raw_file:
    output = open(raw_file, "w")
    output.write(sources_bytes)
    output.close()

  if startup_blob:
    WriteStartupBlob(sources, startup_blob);

  # Emit resulting source file.
  output = open(target, "w")
  output.write(HEADER_TEMPLATE % metadata)
  output.close()


def main():
  parser = optparse.OptionParser()
  parser.add_option("--raw", action="store",
                    help="file to write the processed sources array to.")
  parser.add_option("--startup_blob", action="store",
                    help="file to write the startup blob to.")
  parser.set_usage("""js2c out.cc type compression sources.js ...
      out.cc: C code to be generated.
      type: type parameter for NativesCollection template.
      compression: type of compression used. [off|bz2]
      sources.js: JS internal sources or macros.py.""")
  (options, args) = parser.parse_args()

  JS2C(args[3:], args[0], args[1], args[2], options.raw, options.startup_blob)


if __name__ == "__main__":
  main()
