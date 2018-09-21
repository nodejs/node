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

import os, re
import optparse
import jsmin
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


def RemoveCommentsEmptyLinesAndWhitespace(lines):
  lines = re.sub(r'\n+', '\n', lines) # empty lines
  lines = re.sub(r'//.*\n', '\n', lines) # end-of-line comments
  lines = re.sub(re.compile(r'/\*.*?\*/', re.DOTALL), '', lines) # comments.
  lines = re.sub(r'\s+\n', '\n', lines) # trailing whitespace
  lines = re.sub(r'\n\s+', '\n', lines) # initial whitespace
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
INVALID_ERROR_MESSAGE_PATTERN = re.compile(
    r'Make(?!Generic)\w*Error\(([kA-Z]\w+)')
NEW_ERROR_PATTERN = re.compile(r'new \$\w*Error\((?!\))')

def Validate(lines):
  # Because of simplified context setup, eval and with is not
  # allowed in the natives files.
  if EVAL_PATTERN.search(lines):
    raise Error("Eval disallowed in natives.")
  if WITH_PATTERN.search(lines):
    raise Error("With statements disallowed in natives.")
  invalid_error = INVALID_ERROR_MESSAGE_PATTERN.search(lines)
  if invalid_error:
    raise Error("Unknown error message template '%s'" % invalid_error.group(1))
  if NEW_ERROR_PATTERN.search(lines):
    raise Error("Error constructed without message template.")
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
      if arg_index[0] >= len(macro.args):
        lineno = lines.count(os.linesep, 0, start) + 1
        raise Error('line %s: Too many arguments for macro "%s"' % (lineno, name_pattern.pattern))
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
    if arg_index[0] < len(macro.args) -1:
      lineno = lines.count(os.linesep, 0, start) + 1
      raise Error('line %s: Too few arguments for macro "%s"' % (lineno, name_pattern.pattern))
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
    # Keys could be substrings of earlier values. To avoid unintended
    # clobbering, apply all replacements simultaneously.
    any_key_pattern = "|".join(re.escape(k) for k in mapping.iterkeys())
    def replace(match):
      return mapping[match.group(0)]
    return re.sub(any_key_pattern, replace, self.body)

CONST_PATTERN = re.compile(r'^define\s+([a-zA-Z0-9_]+)\s*=\s*([^;]*);$')
MACRO_PATTERN = re.compile(r'^macro\s+([a-zA-Z0-9_]+)\s*\(([^)]*)\)\s*=\s*([^;]*);$')


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
        raise Error("Illegal line: " + line)
  return (constants, macros)


TEMPLATE_PATTERN = re.compile(r'^\s+T\(([A-Z][a-zA-Z0-9]*),')

def ReadMessageTemplates(lines):
  templates = []
  index = 0
  for line in lines.split('\n'):
    template_match = TEMPLATE_PATTERN.match(line)
    if template_match:
      name = "k%s" % template_match.group(1)
      value = index
      index = index + 1
      templates.append((re.compile("\\b%s\\b" % name), value))
  return templates

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

    # advance position to where the macro definition was
    pos = macro_match.start()

    def non_expander(s):
      return s
    lines = ExpandMacroDefinition(lines, pos, name_pattern, macro, non_expander)


INLINE_CONSTANT_PATTERN = re.compile(r'define\s+([a-zA-Z0-9_]+)\s*=\s*([^;\n]+);\n')

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

    # advance position to where the constant definition was
    pos = const_match.start()


HEADER_TEMPLATE = """\
// Copyright 2011 Google Inc. All Rights Reserved.

// This file was generated from .js source files by GYP.  If you
// want to make changes to this file you should either change the
// javascript source files or the GYP script.

#include "src/v8.h"
#include "src/snapshot/natives.h"
#include "src/utils.h"

namespace v8 {
namespace internal {

%(sources_declaration)s\

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
  Vector<const char> NativesCollection<%(type)s>::GetScriptSource(int index) {
%(get_script_source_cases)s\
    return Vector<const char>("", 0);
  }

  template <>
  Vector<const char> NativesCollection<%(type)s>::GetScriptName(int index) {
%(get_script_name_cases)s\
    return Vector<const char>("", 0);
  }

  template <>
  Vector<const char> NativesCollection<%(type)s>::GetScriptsSource() {
    return Vector<const char>(sources, %(total_length)i);
  }
}  // internal
}  // v8
"""

SOURCES_DECLARATION = """\
  static const char sources[] = { %s };
"""


GET_INDEX_CASE = """\
    if (strcmp(name, "%(id)s") == 0) return %(i)i;
"""


GET_SCRIPT_SOURCE_CASE = """\
    if (index == %(i)i) return Vector<const char>(sources + %(offset)i, %(source_length)i);
"""


GET_SCRIPT_NAME_CASE = """\
    if (index == %(i)i) return Vector<const char>("%(name)s", %(length)i);
"""


def BuildFilterChain(macro_filename, message_template_file):
  """Build the chain of filter functions to be applied to the sources.

  Args:
    macro_filename: Name of the macro file, if any.

  Returns:
    A function (string -> string) that processes a source file.
  """
  filter_chain = []

  if macro_filename:
    (consts, macros) = ReadMacros(ReadFile(macro_filename))
    filter_chain.append(lambda l: ExpandMacros(l, macros))
    filter_chain.append(lambda l: ExpandConstants(l, consts))

  if message_template_file:
    message_templates = ReadMessageTemplates(ReadFile(message_template_file))
    filter_chain.append(lambda l: ExpandConstants(l, message_templates))

  filter_chain.extend([
    RemoveCommentsEmptyLinesAndWhitespace,
    ExpandInlineMacros,
    ExpandInlineConstants,
    Validate,
    jsmin.JavaScriptMinifier().JSMinify
  ])

  def chain(f1, f2):
    return lambda x: f2(f1(x))

  return reduce(chain, filter_chain)

def BuildExtraFilterChain():
  return lambda x: RemoveCommentsEmptyLinesAndWhitespace(Validate(x))

class Sources:
  def __init__(self):
    self.names = []
    self.modules = []
    self.is_debugger_id = []


def IsDebuggerFile(filename):
  return os.path.basename(os.path.dirname(filename)) == "debug"

def IsMacroFile(filename):
  return filename.endswith("macros.py")

def IsMessageTemplateFile(filename):
  return filename.endswith("messages.h")


def PrepareSources(source_files, native_type, emit_js):
  """Read, prepare and assemble the list of source files.

  Args:
    source_files: List of JavaScript-ish source files. A file named macros.py
        will be treated as a list of macros.
    native_type: String corresponding to a NativeType enum value, allowing us
        to treat different types of sources differently.
    emit_js: True if we should skip the byte conversion and just leave the
        sources as JS strings.

  Returns:
    An instance of Sources.
  """
  macro_file = None
  macro_files = filter(IsMacroFile, source_files)
  assert len(macro_files) in [0, 1]
  if macro_files:
    source_files.remove(macro_files[0])
    macro_file = macro_files[0]

  message_template_file = None
  message_template_files = filter(IsMessageTemplateFile, source_files)
  assert len(message_template_files) in [0, 1]
  if message_template_files:
    source_files.remove(message_template_files[0])
    message_template_file = message_template_files[0]

  filters = None
  if native_type in ("EXTRAS", "EXPERIMENTAL_EXTRAS"):
    filters = BuildExtraFilterChain()
  else:
    filters = BuildFilterChain(macro_file, message_template_file)

  # Sort 'debugger' sources first.
  source_files = sorted(source_files,
                        lambda l,r: IsDebuggerFile(r) - IsDebuggerFile(l))

  source_files_and_contents = [(f, ReadFile(f)) for f in source_files]

  # Have a single not-quite-empty source file if there are none present;
  # otherwise you get errors trying to compile an empty C++ array.
  # It cannot be empty (or whitespace, which gets trimmed to empty), as
  # the deserialization code assumes each file is nonempty.
  if not source_files_and_contents:
    source_files_and_contents = [("dummy.js", "(function() {})")]

  result = Sources()

  for (source, contents) in source_files_and_contents:
    try:
      lines = filters(contents)
    except Error as e:
      raise Error("In file %s:\n%s" % (source, str(e)))

    result.modules.append(lines)

    is_debugger = IsDebuggerFile(source)
    result.is_debugger_id.append(is_debugger)

    name = os.path.basename(source)[:-3]
    result.names.append(name)

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
  get_script_source_cases = []
  offset = 0
  for i in xrange(len(sources.modules)):
    native_name = "native %s.js" % sources.names[i]
    d = {
        "i": i,
        "id": sources.names[i],
        "name": native_name,
        "length": len(native_name),
        "offset": offset,
        "source_length": len(sources.modules[i]),
    }
    get_index_cases.append(GET_INDEX_CASE % d)
    get_script_name_cases.append(GET_SCRIPT_NAME_CASE % d)
    get_script_source_cases.append(GET_SCRIPT_SOURCE_CASE % d)
    offset += len(sources.modules[i])
  assert offset == len(raw_sources)

  metadata = {
    "builtin_count": len(sources.modules),
    "debugger_count": sum(sources.is_debugger_id),
    "sources_declaration": SOURCES_DECLARATION % ToCArray(source_bytes),
    "total_length": total_length,
    "get_index_cases": "".join(get_index_cases),
    "get_script_source_cases": "".join(get_script_source_cases),
    "get_script_name_cases": "".join(get_script_name_cases),
    "type": native_type,
  }
  return metadata


def PutInt(blob_file, value):
  assert(value >= 0 and value < (1 << 28))
  if (value < 1 << 6):
    size = 1
  elif (value < 1 << 14):
    size = 2
  elif (value < 1 << 22):
    size = 3
  else:
    size = 4
  value_with_length = (value << 2) | (size - 1)

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


def JS2C(sources, target, native_type, raw_file, startup_blob, emit_js):
  prepared_sources = PrepareSources(sources, native_type, emit_js)
  sources_output = "".join(prepared_sources.modules)
  metadata = BuildMetadata(prepared_sources, sources_output, native_type)

  # Optionally emit raw file.
  if raw_file:
    output = open(raw_file, "w")
    output.write(sources_output)
    output.close()

  if startup_blob:
    WriteStartupBlob(prepared_sources, startup_blob)

  # Emit resulting source file.
  output = open(target, "w")
  if emit_js:
    output.write(sources_output)
  else:
    output.write(HEADER_TEMPLATE % metadata)
  output.close()


def main():
  parser = optparse.OptionParser()
  parser.add_option("--raw",
                    help="file to write the processed sources array to.")
  parser.add_option("--startup_blob",
                    help="file to write the startup blob to.")
  parser.add_option("--js",
                    help="writes a JS file output instead of a C file",
                    action="store_true", default=False, dest='js')
  parser.add_option("--nojs", action="store_false", default=False, dest='js')
  parser.set_usage("""js2c out.cc type sources.js ...
        out.cc: C code to be generated.
        type: type parameter for NativesCollection template.
        sources.js: JS internal sources or macros.py.""")
  (options, args) = parser.parse_args()
  JS2C(args[2:],
       args[0],
       args[1],
       options.raw,
       options.startup_blob,
       options.js)


if __name__ == "__main__":
  main()
