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


import test
import os
from os.path import join, exists
import urllib
import hashlib
import tarfile


TEST_262_ARCHIVE_REVISION = '3a890174343c'  # This is the r309 revision.
TEST_262_ARCHIVE_MD5 = 'be5d4cfbe69cef70430907b8f3a92b50'
TEST_262_URL = 'http://hg.ecmascript.org/tests/test262/archive/%s.tar.bz2'
TEST_262_HARNESS = ['sta.js']


class Test262TestCase(test.TestCase):

  def __init__(self, filename, path, context, root, mode, framework):
    super(Test262TestCase, self).__init__(context, path, mode)
    self.filename = filename
    self.framework = framework
    self.root = root

  def IsNegative(self):
    return '@negative' in self.GetSource()

  def GetLabel(self):
    return "%s test262 %s" % (self.mode, self.GetName())

  def IsFailureOutput(self, output):
    if output.exit_code != 0:
      return True
    return 'FAILED!' in output.stdout

  def GetCommand(self):
    result = self.context.GetVmCommand(self, self.mode)
    result += self.framework
    result.append(self.filename)
    return result

  def GetName(self):
    return self.path[-1]

  def GetSource(self):
    return open(self.filename).read()


class Test262TestConfiguration(test.TestConfiguration):

  def __init__(self, context, root):
    super(Test262TestConfiguration, self).__init__(context, root)

  def ListTests(self, current_path, path, mode, variant_flags):
    testroot = join(self.root, 'data', 'test', 'suite')
    harness = [join(self.root, 'data', 'test', 'harness', f)
                   for f in TEST_262_HARNESS]
    harness += [join(self.root, 'harness-adapt.js')]
    tests = []
    for root, dirs, files in os.walk(testroot):
      for dotted in [x for x in dirs if x.startswith('.')]:
        dirs.remove(dotted)
      dirs.sort()
      root_path = root[len(self.root):].split(os.path.sep)
      root_path = current_path + [x for x in root_path if x]
      files.sort()
      for file in files:
        if file.endswith('.js'):
          test_path = ['test262', file[:-3]]
          if self.Contains(path, test_path):
            test = Test262TestCase(join(root, file), test_path, self.context,
                                   self.root, mode, harness)
            tests.append(test)
    return tests

  def DownloadData(self):
    revision = TEST_262_ARCHIVE_REVISION
    archive_url = TEST_262_URL % revision
    archive_name = join(self.root, 'test262-%s.tar.bz2' % revision)
    directory_name = join(self.root, "test262-%s" % revision)
    if not exists(directory_name) or not exists(archive_name):
      if not exists(archive_name):
        print "Downloading test data from %s ..." % archive_url
        urllib.urlretrieve(archive_url, archive_name)
      if not exists(directory_name):
        print "Extracting test262-%s.tar.bz2 ..." % revision
        md5 = hashlib.md5()
        with open(archive_name,'rb') as f:
          for chunk in iter(lambda: f.read(8192), ''):
            md5.update(chunk)
        if md5.hexdigest() != TEST_262_ARCHIVE_MD5:
          raise Exception("Hash mismatch of test data file")
        archive = tarfile.open(archive_name, 'r:bz2')
        archive.extractall(join(self.root))
      if not exists(join(self.root, 'data')):
        os.symlink(directory_name, join(self.root, 'data'))

  def GetBuildRequirements(self):
    return ['d8']

  def GetTestStatus(self, sections, defs):
    status_file = join(self.root, 'test262.status')
    if exists(status_file):
      test.ReadConfigurationInto(status_file, sections, defs)


def GetConfiguration(context, root):
  return Test262TestConfiguration(context, root)
