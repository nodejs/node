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

import test
import os
from os.path import join, exists, basename, isdir
import re

FLAGS_PATTERN = re.compile(r"//\s+Flags:(.*)")

class MessageTestCase(test.TestCase):

  def __init__(self, path, file, expected, arch, mode, context, config):
    super(MessageTestCase, self).__init__(context, path, arch, mode)
    self.file = file
    self.expected = expected
    self.config = config
    self.arch = arch
    self.mode = mode

  def IgnoreLine(self, str):
    """Ignore empty lines and valgrind output."""
    if not str.strip(): return True
    else: return str.startswith('==') or str.startswith('**')

  def IsFailureOutput(self, output):
    f = file(self.expected)
    # Skip initial '#' comment and spaces
    #for line in f:
    #  if (not line.startswith('#')) and (not line.strip()):
    #    break
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
    outlines = [ s for s in raw_lines if not self.IgnoreLine(s) ]
    if len(outlines) != len(patterns):
      print "length differs."
      print "expect=%d" % len(patterns)
      print "actual=%d" % len(outlines)
      print "patterns:"
      for i in xrange(len(patterns)):
        print "pattern = %s" % patterns[i]
      print "outlines:"
      for i in xrange(len(outlines)):
        print "outline = %s" % outlines[i]
      return True
    for i in xrange(len(patterns)):
      if not re.match(patterns[i], outlines[i]):
        print "match failed"
        print "line=%d" % i
        print "expect=%s" % patterns[i]
        print "actual=%s" % outlines[i]
        return True
    return False

  def GetLabel(self):
    return "%s %s" % (self.mode, self.GetName())

  def GetName(self):
    return self.path[-1]

  def GetCommand(self):
    result = [self.config.context.GetVm(self.arch, self.mode)]
    source = open(self.file).read()
    flags_match = FLAGS_PATTERN.search(source)
    if flags_match:
      result += flags_match.group(1).strip().split()
    result.append(self.file)
    return result

  def GetSource(self):
    return (open(self.file).read()
          + "\n--- expected output ---\n"
          + open(self.expected).read())


class MessageTestConfiguration(test.TestConfiguration):

  def __init__(self, context, root):
    super(MessageTestConfiguration, self).__init__(context, root)

  def Ls(self, path):
    if isdir(path):
      return [f for f in os.listdir(path)
              if f.endswith('.js') or f.endswith('.mjs')]
    else:
      return []

  def ListTests(self, current_path, path, arch, mode):
    all_tests = [current_path + [t] for t in self.Ls(self.root)]
    result = []
    for test in all_tests:
      if self.Contains(path, test):
        file_path = join(self.root, reduce(join, test[1:], ''))
        output_path = file_path[:file_path.rfind('.')] + '.out'
        if not exists(output_path):
          raise Exception("Could not find %s" % output_path)
        result.append(MessageTestCase(test, file_path, output_path,
                                      arch, mode, self.context, self))
    return result

  def GetBuildRequirements(self):
    return ['sample', 'sample=shell']

  def GetTestStatus(self, sections, defs):
    status_file = join(self.root, 'message.status')
    if exists(status_file):
      test.ReadConfigurationInto(status_file, sections, defs)


def GetConfiguration(context, root):
  return MessageTestConfiguration(context, root)
