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

try:
  import hashlib
  md5er = hashlib.md5
except ImportError, e:
  import md5
  md5er = md5.new


import optparse
import os
from os.path import abspath, join, dirname, basename, exists
import pickle
import re
import sys
import subprocess
from subprocess import PIPE

# Disabled LINT rules and reason.
# build/include_what_you_use: Started giving false positives for variables
#  named "string" and "map" assuming that you needed to include STL headers.

ENABLED_LINT_RULES = """
build/class
build/deprecated
build/endif_comment
build/forward_decl
build/include_order
build/printf_format
build/storage_class
legal/copyright
readability/boost
readability/braces
readability/casting
readability/check
readability/constructors
readability/fn_size
readability/function
readability/multiline_comment
readability/multiline_string
readability/streams
readability/todo
readability/utf8
runtime/arrays
runtime/casting
runtime/deprecated_fn
runtime/explicit
runtime/int
runtime/memset
runtime/mutex
runtime/nonconf
runtime/printf
runtime/printf_format
runtime/references
runtime/rtti
runtime/sizeof
runtime/string
runtime/virtual
runtime/vlog
whitespace/blank_line
whitespace/braces
whitespace/comma
whitespace/comments
whitespace/ending_newline
whitespace/indent
whitespace/labels
whitespace/line_length
whitespace/newline
whitespace/operators
whitespace/parens
whitespace/tab
whitespace/todo
""".split()


class FileContentsCache(object):

  def __init__(self, sums_file_name):
    self.sums = {}
    self.sums_file_name = sums_file_name

  def Load(self):
    try:
      sums_file = None
      try:
        sums_file = open(self.sums_file_name, 'r')
        self.sums = pickle.load(sums_file)
      except IOError:
        # File might not exist, this is OK.
        pass
    finally:
      if sums_file:
        sums_file.close()

  def Save(self):
    try:
      sums_file = open(self.sums_file_name, 'w')
      pickle.dump(self.sums, sums_file)
    finally:
      sums_file.close()

  def FilterUnchangedFiles(self, files):
    changed_or_new = []
    for file in files:
      try:
        handle = open(file, "r")
        file_sum = md5er(handle.read()).digest()
        if not file in self.sums or self.sums[file] != file_sum:
          changed_or_new.append(file)
          self.sums[file] = file_sum
      finally:
        handle.close()
    return changed_or_new

  def RemoveFile(self, file):
    if file in self.sums:
      self.sums.pop(file)


class SourceFileProcessor(object):
  """
  Utility class that can run through a directory structure, find all relevant
  files and invoke a custom check on the files.
  """

  def Run(self, path):
    all_files = []
    for file in self.GetPathsToSearch():
      all_files += self.FindFilesIn(join(path, file))
    if not self.ProcessFiles(all_files, path):
      return False
    return True

  def IgnoreDir(self, name):
    return name.startswith('.') or name == 'data' or name == 'sputniktests'

  def IgnoreFile(self, name):
    return name.startswith('.')

  def FindFilesIn(self, path):
    result = []
    for (root, dirs, files) in os.walk(path):
      for ignored in [x for x in dirs if self.IgnoreDir(x)]:
        dirs.remove(ignored)
      for file in files:
        if not self.IgnoreFile(file) and self.IsRelevant(file):
          result.append(join(root, file))
    return result


class CppLintProcessor(SourceFileProcessor):
  """
  Lint files to check that they follow the google code style.
  """

  def IsRelevant(self, name):
    return name.endswith('.cc') or name.endswith('.h')

  def IgnoreDir(self, name):
    return (super(CppLintProcessor, self).IgnoreDir(name)
              or (name == 'third_party'))

  IGNORE_LINT = ['flag-definitions.h']

  def IgnoreFile(self, name):
    return (super(CppLintProcessor, self).IgnoreFile(name)
              or (name in CppLintProcessor.IGNORE_LINT))

  def GetPathsToSearch(self):
    return ['src', 'preparser', 'include', 'samples', join('test', 'cctest')]

  def ProcessFiles(self, files, path):
    good_files_cache = FileContentsCache('.cpplint-cache')
    good_files_cache.Load()
    files = good_files_cache.FilterUnchangedFiles(files)
    if len(files) == 0:
      print 'No changes in files detected. Skipping cpplint check.'
      return True

    filt = '-,' + ",".join(['+' + n for n in ENABLED_LINT_RULES])
    command = ['cpplint.py', '--filter', filt] + join(files)
    local_cpplint = join(path, "tools", "cpplint.py")
    if exists(local_cpplint):
      command = ['python', local_cpplint, '--filter', filt] + join(files)

    process = subprocess.Popen(command, stderr=subprocess.PIPE)
    LINT_ERROR_PATTERN = re.compile(r'^(.+)[:(]\d+[:)]')
    while True:
      out_line = process.stderr.readline()
      if out_line == '' and process.poll() != None:
        break
      sys.stderr.write(out_line)
      m = LINT_ERROR_PATTERN.match(out_line)
      if m:
        good_files_cache.RemoveFile(m.group(1))

    good_files_cache.Save()
    return process.returncode == 0


COPYRIGHT_HEADER_PATTERN = re.compile(
    r'Copyright [\d-]*20[0-1][0-9] the V8 project authors. All rights reserved.')

class SourceProcessor(SourceFileProcessor):
  """
  Check that all files include a copyright notice and no trailing whitespaces.
  """

  RELEVANT_EXTENSIONS = ['.js', '.cc', '.h', '.py', '.c', 'SConscript',
      'SConstruct', '.status', '.gyp', '.gypi']

  # Overwriting the one in the parent class.
  def FindFilesIn(self, path):
    if os.path.exists(path+'/.git'):
      output = subprocess.Popen('git ls-files --full-name',
                                stdout=PIPE, cwd=path, shell=True)
      result = []
      for file in output.stdout.read().split():
        for dir_part in os.path.dirname(file).split(os.sep):
          if self.IgnoreDir(dir_part):
            break
        else:
          if self.IsRelevant(file) and not self.IgnoreFile(file):
            result.append(join(path, file))
      if output.wait() == 0:
        return result
    return super(SourceProcessor, self).FindFilesIn(path)

  def IsRelevant(self, name):
    for ext in SourceProcessor.RELEVANT_EXTENSIONS:
      if name.endswith(ext):
        return True
    return False

  def GetPathsToSearch(self):
    return ['.']

  def IgnoreDir(self, name):
    return (super(SourceProcessor, self).IgnoreDir(name)
              or (name == 'third_party')
              or (name == 'gyp')
              or (name == 'out')
              or (name == 'obj'))

  IGNORE_COPYRIGHTS = ['cpplint.py',
                       'earley-boyer.js',
                       'raytrace.js',
                       'crypto.js',
                       'libraries.cc',
                       'libraries-empty.cc',
                       'jsmin.py',
                       'regexp-pcre.js']
  IGNORE_TABS = IGNORE_COPYRIGHTS + ['unicode-test.js', 'html-comments.js']

  def ProcessContents(self, name, contents):
    result = True
    base = basename(name)
    if not base in SourceProcessor.IGNORE_TABS:
      if '\t' in contents:
        print "%s contains tabs" % name
        result = False
    if not base in SourceProcessor.IGNORE_COPYRIGHTS:
      if not COPYRIGHT_HEADER_PATTERN.search(contents):
        print "%s is missing a correct copyright header." % name
        result = False
    ext = base.split('.').pop()
    if ' \n' in contents or contents.endswith(' '):
      line = 0
      lines = []
      parts = contents.split(' \n')
      if not contents.endswith(' '):
        parts.pop()
      for part in parts:
        line += part.count('\n') + 1
        lines.append(str(line))
      linenumbers = ', '.join(lines)
      if len(lines) > 1:
        print "%s has trailing whitespaces in lines %s." % (name, linenumbers)
      else:
        print "%s has trailing whitespaces in line %s." % (name, linenumbers)
      result = False
    return result

  def ProcessFiles(self, files, path):
    success = True
    violations = 0
    for file in files:
      try:
        handle = open(file)
        contents = handle.read()
        if not self.ProcessContents(file, contents):
          success = False
          violations += 1
      finally:
        handle.close()
    print "Total violating files: %s" % violations
    return success


def GetOptions():
  result = optparse.OptionParser()
  result.add_option('--no-lint', help="Do not run cpplint", default=False,
                    action="store_true")
  return result


def Main():
  workspace = abspath(join(dirname(sys.argv[0]), '..'))
  parser = GetOptions()
  (options, args) = parser.parse_args()
  success = True
  print "Running C++ lint check..."
  if not options.no_lint:
    success = CppLintProcessor().Run(workspace) and success
  print "Running copyright header and trailing whitespaces check..."
  success = SourceProcessor().Run(workspace) and success
  if success:
    return 0
  else:
    return 1


if __name__ == '__main__':
  sys.exit(Main())
