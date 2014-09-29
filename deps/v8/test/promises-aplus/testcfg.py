# Copyright 2014 the V8 project authors. All rights reserved.
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
# 'AS IS' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


import hashlib
import os
import shutil
import sys
import tarfile

from testrunner.local import testsuite
from testrunner.local import utils
from testrunner.objects import testcase


SINON_TAG = '1.7.3'
SINON_NAME = 'sinon'
SINON_FILENAME = 'sinon.js'
SINON_URL = 'http://sinonjs.org/releases/sinon-' + SINON_TAG + '.js'
SINON_HASH = 'b7ab4dd9a1a2cf0460784af3728ad15caf4bbea923f680c5abde5c8332f35984'

TEST_TAG = '2.0.3'
TEST_ARCHIVE_TOP = 'promises-tests-' + TEST_TAG
TEST_NAME = 'promises-tests'
TEST_ARCHIVE = TEST_NAME + '.tar.gz'
TEST_URL = 'https://github.com/promises-aplus/promises-tests/archive/' + \
    TEST_TAG + '.tar.gz'
TEST_ARCHIVE_HASH = \
    'e446ca557ac5836dd439fecd19689c243a28b1d5a6644dd7fed4274d0fa67270'


class PromiseAplusTestSuite(testsuite.TestSuite):

  def __init__(self, name, root):
    self.root = root
    self.test_files_root = os.path.join(self.root, TEST_NAME, 'lib', 'tests')
    self.name = name
    self.helper_files_pre = [
      os.path.join(root, 'lib', name) for name in
      ['global.js', 'require.js', 'mocha.js', 'adapter.js']
    ]
    self.helper_files_post = [
      os.path.join(root, 'lib', name) for name in
      ['run-tests.js']
    ]

  def CommonTestName(self, testcase):
    return testcase.path.split(os.path.sep)[-1]

  def ListTests(self, context):
    return [testcase.TestCase(self, fname[:-len('.js')]) for fname in
            os.listdir(os.path.join(self.root, TEST_NAME, 'lib', 'tests'))
            if fname.endswith('.js')]

  def GetFlagsForTestCase(self, testcase, context):
    return (testcase.flags + context.mode_flags + ['--harmony'] +
            self.helper_files_pre +
            [os.path.join(self.test_files_root, testcase.path + '.js')] +
            self.helper_files_post)

  def GetSourceForTest(self, testcase):
    filename = os.path.join(self.root, TEST_NAME,
                            'lib', 'tests', testcase.path + '.js')
    with open(filename) as f:
      return f.read()

  def IsNegativeTest(self, testcase):
    return '@negative' in self.GetSourceForTest(testcase)

  def IsFailureOutput(self, output, testpath):
    if output.exit_code != 0:
      return True
    return not 'All tests have run.' in output.stdout or \
           'FAIL:' in output.stdout

  def DownloadTestData(self):
    archive = os.path.join(self.root, TEST_ARCHIVE)
    directory = os.path.join(self.root, TEST_NAME)
    if not os.path.exists(archive):
      print('Downloading {0} from {1} ...'.format(TEST_NAME, TEST_URL))
      utils.URLRetrieve(TEST_URL, archive)
      if os.path.exists(directory):
        shutil.rmtree(directory)

    if not os.path.exists(directory):
      print('Extracting {0} ...'.format(TEST_ARCHIVE))
      hash = hashlib.sha256()
      with open(archive, 'rb') as f:
        for chunk in iter(lambda: f.read(8192), ''):
          hash.update(chunk)
        if hash.hexdigest() != TEST_ARCHIVE_HASH:
          os.remove(archive)
          raise Exception('Hash mismatch of test data file')
      archive = tarfile.open(archive, 'r:gz')
      if sys.platform in ('win32', 'cygwin'):
        # Magic incantation to allow longer path names on Windows.
        archive.extractall(u'\\\\?\\%s' % self.root)
      else:
        archive.extractall(self.root)
      shutil.move(os.path.join(self.root, TEST_ARCHIVE_TOP), directory)

  def DownloadSinon(self):
    directory = os.path.join(self.root, SINON_NAME)
    if not os.path.exists(directory):
      os.mkdir(directory)
    path = os.path.join(directory, SINON_FILENAME)
    if not os.path.exists(path):
      utils.URLRetrieve(SINON_URL, path)
    hash = hashlib.sha256()
    with open(path, 'rb') as f:
      for chunk in iter(lambda: f.read(8192), ''):
        hash.update(chunk)
    if hash.hexdigest() != SINON_HASH:
      os.remove(path)
      raise Exception('Hash mismatch of test data file')

  def DownloadData(self):
    self.DownloadTestData()
    self.DownloadSinon()


def GetSuite(name, root):
  return PromiseAplusTestSuite(name, root)
