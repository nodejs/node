# Copyright 2008 the V8 project authors. All rights reserved.
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

from __future__ import print_function

import test
import os
from os.path import join, exists, basename, dirname, isdir
import re
import sys
import utils
from functools import reduce

FLAGS_PATTERN = re.compile(r"//\s+Flags:(.*)")
PTY_HELPER = join(dirname(__file__), '../../tools/pseudo-tty.py')
ENV_PATTERN = re.compile(r"//\s+Env:(.*)")

class TTYTestCase(test.TestCase):

  def __init__(self, path, file, expected, input_arg, arch, mode, context, config):
    super(TTYTestCase, self).__init__(context, path, arch, mode)
    self.file = file
    self.expected = expected
    self.input = input_arg
    self.config = config
    self.arch = arch
    self.mode = mode
    self.parallel = True

  def IgnoreLine(self, str_arg):
    """Ignore empty lines and valgrind output."""
    if not str_arg.strip(): return True
    else: return str_arg.startswith('==') or str_arg.startswith('**')

  def IsFailureOutput(self, output):
    f = open(self.expected)
    # Convert output lines to regexps that we can match
    env = { 'basename': basename(self.file) }
    patterns = [ ]
    for line in f:
      if not line.strip():
        continue
      pattern = re.escape(line.rstrip() % env)
      pattern = pattern.replace('\\*', '.*')
      pattern = '^%s$' % pattern
      patterns.append(pattern)
    # Compare actual output with the expected
    raw_lines = (output.stdout + output.stderr).split('\n')
    outlines = [ s.rstrip() for s in raw_lines if not self.IgnoreLine(s) ]
    if len(outlines) != len(patterns):
      print(" length differs.")
      print("expect=%d" % len(patterns))
      print("actual=%d" % len(outlines))
      print("patterns:")
      for i in range(len(patterns)):
        print("pattern = %s" % patterns[i])
      print("outlines:")
      for i in range(len(outlines)):
        print("outline = %s" % outlines[i])
      return True
    for i in range(len(patterns)):
      if not re.match(patterns[i], outlines[i]):
        print(" match failed")
        print("line=%d" % i)
        print("expect=%s" % patterns[i])
        print("actual=%s" % outlines[i])
        return True
    return False

  def _parse_source_env(self, source):
    env_match = ENV_PATTERN.search(source)
    env = {}
    if env_match:
      for env_pair in env_match.group(1).strip().split():
        var, value = env_pair.split('=')
        env[var] = value
    return env

  def GetLabel(self):
    return "%s %s" % (self.mode, self.GetName())

  def GetName(self):
    return self.path[-1]

  def GetRunConfiguration(self):
    result = [self.config.context.GetVm(self.arch, self.mode)]
    source = open(self.file).read()
    flags_match = FLAGS_PATTERN.search(source)
    envs = self._parse_source_env(source)
    if flags_match:
      result += flags_match.group(1).strip().split()
    result.append(self.file)
    return {
        'command': result,
        'envs': envs
    }

  def GetSource(self):
    return (open(self.file).read()
          + "\n--- expected output ---\n"
          + open(self.expected).read())

  def RunCommand(self, command, env):
    fd = None
    if self.input is not None and exists(self.input):
      fd = os.open(self.input, os.O_RDONLY)
    full_command = self.context.processor(command)
    full_command = [sys.executable, PTY_HELPER] + full_command
    output = test.Execute(full_command,
                     self.context,
                     self.context.GetTimeout(self.mode),
                     env,
                     stdin=fd)
    if fd is not None:
      os.close(fd)
    return test.TestOutput(self,
                      full_command,
                      output,
                      self.context.store_unexpected_output)


class TTYTestConfiguration(test.TestConfiguration):
  def Ls(self, path):
    if isdir(path):
        return [f[:-3] for f in os.listdir(path) if f.endswith('.js')]
    else:
        return []

  def ListTests(self, current_path, path, arch, mode):
    all_tests = [current_path + [t] for t in self.Ls(self.root)]
    result = []
    # Skip these tests on Windows, as pseudo terminals are not available
    if utils.IsWindows():
      print ("Skipping pseudo-tty tests, as pseudo terminals are not available"
             " on Windows.")
      return result
    for tst in all_tests:
      if self.Contains(path, tst):
        file_prefix = join(self.root, reduce(join, tst[1:], ""))
        file_path = file_prefix + ".js"
        input_path = file_prefix + ".in"
        output_path = file_prefix + ".out"
        if not exists(output_path):
          raise Exception("Could not find %s" % output_path)
        result.append(TTYTestCase(tst, file_path, output_path,
                                  input_path, arch, mode, self.context, self))
    return result

  def GetBuildRequirements(self):
    return ['sample', 'sample=shell']


def GetConfiguration(context, root):
  return TTYTestConfiguration(context, root, 'pseudo-tty')
