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

try:
  import hashlib
  md5er = hashlib.md5
except ImportError, e:
  import md5
  md5er = md5.new


import json
import optparse
import os
from os.path import abspath, join, dirname, basename, exists
import pickle
import re
import sys
import subprocess
import multiprocessing
from subprocess import PIPE

from testrunner.local import statusfile
from testrunner.local import testsuite
from testrunner.local import utils

# Special LINT rules diverging from default and reason.
# build/header_guard: Our guards have the form "V8_FOO_H_", not "SRC_FOO_H_".
# build/include_what_you_use: Started giving false positives for variables
#   named "string" and "map" assuming that you needed to include STL headers.
# TODO(bmeurer): Fix and re-enable readability/check

LINT_RULES = """
-build/header_guard
-build/include_what_you_use
-build/namespaces
-readability/check
+readability/streams
-runtime/references
""".split()

LINT_OUTPUT_PATTERN = re.compile(r'^.+[:(]\d+[:)]|^Done processing')
FLAGS_LINE = re.compile("//\s*Flags:.*--([A-z0-9-])+_[A-z0-9].*\n")

def CppLintWorker(command):
  try:
    process = subprocess.Popen(command, stderr=subprocess.PIPE)
    process.wait()
    out_lines = ""
    error_count = -1
    while True:
      out_line = process.stderr.readline()
      if out_line == '' and process.poll() != None:
        if error_count == -1:
          print "Failed to process %s" % command.pop()
          return 1
        break
      m = LINT_OUTPUT_PATTERN.match(out_line)
      if m:
        out_lines += out_line
        error_count += 1
    sys.stdout.write(out_lines)
    return error_count
  except KeyboardInterrupt:
    process.kill()
  except:
    print('Error running cpplint.py. Please make sure you have depot_tools' +
          ' in your $PATH. Lint check skipped.')
    process.kill()


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
      except:
        # Cannot parse pickle for any reason. Not much we can do about it.
        pass
    finally:
      if sums_file:
        sums_file.close()

  def Save(self):
    try:
      sums_file = open(self.sums_file_name, 'w')
      pickle.dump(self.sums, sums_file)
    except:
      # Failed to write pickle. Try to clean-up behind us.
      if sums_file:
        sums_file.close()
      try:
        os.unlink(self.sums_file_name)
      except:
        pass
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
    return (name.startswith('.') or
            name in ('buildtools', 'data', 'gmock', 'gtest', 'kraken',
                     'octane', 'sunspider'))

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
    return ['src', 'include', 'samples', join('test', 'cctest'),
            join('test', 'unittests')]

  def GetCpplintScript(self, prio_path):
    for path in [prio_path] + os.environ["PATH"].split(os.pathsep):
      path = path.strip('"')
      cpplint = os.path.join(path, "cpplint.py")
      if os.path.isfile(cpplint):
        return cpplint

    return None

  def ProcessFiles(self, files, path):
    good_files_cache = FileContentsCache('.cpplint-cache')
    good_files_cache.Load()
    files = good_files_cache.FilterUnchangedFiles(files)
    if len(files) == 0:
      print 'No changes in files detected. Skipping cpplint check.'
      return True

    filters = ",".join([n for n in LINT_RULES])
    command = [sys.executable, 'cpplint.py', '--filter', filters]
    cpplint = self.GetCpplintScript(join(path, "tools"))
    if cpplint is None:
      print('Could not find cpplint.py. Make sure '
            'depot_tools is installed and in the path.')
      sys.exit(1)

    command = [sys.executable, cpplint, '--filter', filters]

    commands = join([command + [file] for file in files])
    count = multiprocessing.cpu_count()
    pool = multiprocessing.Pool(count)
    try:
      results = pool.map_async(CppLintWorker, commands).get(999999)
    except KeyboardInterrupt:
      print "\nCaught KeyboardInterrupt, terminating workers."
      sys.exit(1)

    for i in range(len(files)):
      if results[i] > 0:
        good_files_cache.RemoveFile(files[i])

    total_errors = sum(results)
    print "Total errors found: %d" % total_errors
    good_files_cache.Save()
    return total_errors == 0


COPYRIGHT_HEADER_PATTERN = re.compile(
    r'Copyright [\d-]*20[0-1][0-9] the V8 project authors. All rights reserved.')

class SourceProcessor(SourceFileProcessor):
  """
  Check that all files include a copyright notice and no trailing whitespaces.
  """

  RELEVANT_EXTENSIONS = ['.js', '.cc', '.h', '.py', '.c',
                         '.status', '.gyp', '.gypi']

  # Overwriting the one in the parent class.
  def FindFilesIn(self, path):
    if os.path.exists(path+'/.git'):
      output = subprocess.Popen('git ls-files --full-name',
                                stdout=PIPE, cwd=path, shell=True)
      result = []
      for file in output.stdout.read().split():
        for dir_part in os.path.dirname(file).replace(os.sep, '/').split('/'):
          if self.IgnoreDir(dir_part):
            break
        else:
          if (self.IsRelevant(file) and os.path.exists(file)
              and not self.IgnoreFile(file)):
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
    return (super(SourceProcessor, self).IgnoreDir(name) or
            name in ('third_party', 'gyp', 'out', 'obj', 'DerivedSources'))

  IGNORE_COPYRIGHTS = ['box2d.js',
                       'cpplint.py',
                       'copy.js',
                       'corrections.js',
                       'crypto.js',
                       'daemon.py',
                       'earley-boyer.js',
                       'fannkuch.js',
                       'fasta.js',
                       'jsmin.py',
                       'libraries.cc',
                       'libraries-empty.cc',
                       'lua_binarytrees.js',
                       'memops.js',
                       'poppler.js',
                       'primes.js',
                       'raytrace.js',
                       'regexp-pcre.js',
                       'sqlite.js',
                       'sqlite-change-heap.js',
                       'sqlite-pointer-masking.js',
                       'sqlite-safe-heap.js',
                       'gnuplot-4.6.3-emscripten.js',
                       'zlib.js']
  IGNORE_TABS = IGNORE_COPYRIGHTS + ['unicode-test.js', 'html-comments.js']

  def EndOfDeclaration(self, line):
    return line == "}" or line == "};"

  def StartOfDeclaration(self, line):
    return line.find("//") == 0 or \
           line.find("/*") == 0 or \
           line.find(") {") != -1

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
    if not contents.endswith('\n') or contents.endswith('\n\n'):
      print "%s does not end with a single new line." % name
      result = False
    # Sanitize flags for fuzzer.
    if "mjsunit" in name:
      match = FLAGS_LINE.search(contents)
      if match:
        print "%s Flags should use '-' (not '_')" % name
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


def CheckExternalReferenceRegistration(workspace):
  code = subprocess.call(
      [sys.executable, join(workspace, "tools", "external-reference-check.py")])
  return code == 0


def _CheckStatusFileForDuplicateKeys(filepath):
  comma_space_bracket = re.compile(", *]")
  lines = []
  with open(filepath) as f:
    for line in f.readlines():
      # Skip all-comment lines.
      if line.lstrip().startswith("#"): continue
      # Strip away comments at the end of the line.
      comment_start = line.find("#")
      if comment_start != -1:
        line = line[:comment_start]
      line = line.strip()
      # Strip away trailing commas within the line.
      line = comma_space_bracket.sub("]", line)
      if len(line) > 0:
        lines.append(line)

  # Strip away trailing commas at line ends. Ugh.
  for i in range(len(lines) - 1):
    if (lines[i].endswith(",") and len(lines[i + 1]) > 0 and
        lines[i + 1][0] in ("}", "]")):
      lines[i] = lines[i][:-1]

  contents = "\n".join(lines)
  # JSON wants double-quotes.
  contents = contents.replace("'", '"')
  # Fill in keywords (like PASS, SKIP).
  for key in statusfile.KEYWORDS:
    contents = re.sub(r"\b%s\b" % key, "\"%s\"" % key, contents)

  status = {"success": True}
  def check_pairs(pairs):
    keys = {}
    for key, value in pairs:
      if key in keys:
        print("%s: Error: duplicate key %s" % (filepath, key))
        status["success"] = False
      keys[key] = True

  json.loads(contents, object_pairs_hook=check_pairs)
  return status["success"]

def CheckStatusFiles(workspace):
  success = True
  suite_paths = utils.GetSuitePaths(join(workspace, "test"))
  for root in suite_paths:
    suite_path = join(workspace, "test", root)
    status_file_path = join(suite_path, root + ".status")
    suite = testsuite.TestSuite.LoadTestSuite(suite_path)
    if suite and exists(status_file_path):
      success &= statusfile.PresubmitCheck(status_file_path)
      success &= _CheckStatusFileForDuplicateKeys(status_file_path)
  return success

def CheckAuthorizedAuthor(input_api, output_api):
  """For non-googler/chromites committers, verify the author's email address is
  in AUTHORS.
  """
  # TODO(maruel): Add it to input_api?
  import fnmatch

  author = input_api.change.author_email
  if not author:
    input_api.logging.info('No author, skipping AUTHOR check')
    return []
  authors_path = input_api.os_path.join(
      input_api.PresubmitLocalPath(), 'AUTHORS')
  valid_authors = (
      input_api.re.match(r'[^#]+\s+\<(.+?)\>\s*$', line)
      for line in open(authors_path))
  valid_authors = [item.group(1).lower() for item in valid_authors if item]
  if not any(fnmatch.fnmatch(author.lower(), valid) for valid in valid_authors):
    input_api.logging.info('Valid authors are %s', ', '.join(valid_authors))
    return [output_api.PresubmitPromptWarning(
        ('%s is not in AUTHORS file. If you are a new contributor, please visit'
        '\n'
        'http://www.chromium.org/developers/contributing-code and read the '
        '"Legal" section\n'
        'If you are a chromite, verify the contributor signed the CLA.') %
        author)]
  return []

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
    success &= CppLintProcessor().Run(workspace)
  print "Running copyright header, trailing whitespaces and " \
        "two empty lines between declarations check..."
  success &= SourceProcessor().Run(workspace)
  success &= CheckExternalReferenceRegistration(workspace)
  success &= CheckStatusFiles(workspace)
  if success:
    return 0
  else:
    return 1


if __name__ == '__main__':
  sys.exit(Main())
