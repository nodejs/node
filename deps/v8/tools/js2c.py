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


def BuildFilterChain():
  """Build the chain of filter functions to be applied to the sources.

  Returns:
    A function (string -> string) that processes a source file.
  """
  filter_chain = [
    RemoveCommentsEmptyLinesAndWhitespace,
    Validate,
  ]

  def chain(f1, f2):
    return lambda x: f2(f1(x))

  return reduce(chain, filter_chain)

def BuildExtraFilterChain():
  return lambda x: RemoveCommentsEmptyLinesAndWhitespace(Validate(x))

class Sources:
  def __init__(self):
    self.names = []
    self.modules = []

def PrepareSources(source_files, native_type, emit_js):
  """Read, prepare and assemble the list of source files.

  Args:
    source_files: List of JavaScript-ish source files.
    native_type: String corresponding to a NativeType enum value, allowing us
        to treat different types of sources differently.
    emit_js: True if we should skip the byte conversion and just leave the
        sources as JS strings.

  Returns:
    An instance of Sources.
  """
  filters = BuildFilterChain()

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

  PutInt(output, len(sources.names))
  for i in xrange(len(sources.names)):
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
        sources.js: JS internal sources.""")
  (options, args) = parser.parse_args()
  JS2C(args[2:],
       args[0],
       args[1],
       options.raw,
       options.startup_blob,
       options.js)


if __name__ == "__main__":
  main()
