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


import hashlib
import os
import shutil
import sys
import tarfile

from testrunner.local import testsuite
from testrunner.local import utils
from testrunner.objects import testcase


TEST_262_ARCHIVE_REVISION = "fbba29f"  # This is the r365 revision.
TEST_262_ARCHIVE_MD5 = "e1ff0db438cc12de8fb6da80621b4ef6"
TEST_262_URL = "https://github.com/tc39/test262/tarball/%s"
TEST_262_HARNESS = ["sta.js", "testBuiltInObject.js", "testIntl.js"]


class Test262TestSuite(testsuite.TestSuite):

  def __init__(self, name, root):
    super(Test262TestSuite, self).__init__(name, root)
    self.testroot = os.path.join(root, "data", "test", "suite")
    self.harness = [os.path.join(self.root, "data", "test", "harness", f)
                    for f in TEST_262_HARNESS]
    self.harness += [os.path.join(self.root, "harness-adapt.js")]

  def CommonTestName(self, testcase):
    return testcase.path.split(os.path.sep)[-1]

  def ListTests(self, context):
    tests = []
    for dirname, dirs, files in os.walk(self.testroot):
      for dotted in [x for x in dirs if x.startswith(".")]:
        dirs.remove(dotted)
      if context.noi18n and "intl402" in dirs:
        dirs.remove("intl402")
      dirs.sort()
      files.sort()
      for filename in files:
        if filename.endswith(".js"):
          testname = os.path.join(dirname[len(self.testroot) + 1:],
                                  filename[:-3])
          case = testcase.TestCase(self, testname)
          tests.append(case)
    return tests

  def GetFlagsForTestCase(self, testcase, context):
    return (testcase.flags + context.mode_flags + self.harness +
            [os.path.join(self.testroot, testcase.path + ".js")])

  def GetSourceForTest(self, testcase):
    filename = os.path.join(self.testroot, testcase.path + ".js")
    with open(filename) as f:
      return f.read()

  def IsNegativeTest(self, testcase):
    return "@negative" in self.GetSourceForTest(testcase)

  def IsFailureOutput(self, output, testpath):
    if output.exit_code != 0:
      return True
    return "FAILED!" in output.stdout

  def DownloadData(self):
    revision = TEST_262_ARCHIVE_REVISION
    archive_url = TEST_262_URL % revision
    archive_name = os.path.join(self.root, "tc39-test262-%s.tar.gz" % revision)
    directory_name = os.path.join(self.root, "data")
    directory_old_name = os.path.join(self.root, "data.old")
    if not os.path.exists(archive_name):
      print "Downloading test data from %s ..." % archive_url
      utils.URLRetrieve(archive_url, archive_name)
      if os.path.exists(directory_name):
        if os.path.exists(directory_old_name):
          shutil.rmtree(directory_old_name)
        os.rename(directory_name, directory_old_name)
    if not os.path.exists(directory_name):
      print "Extracting test262-%s.tar.gz ..." % revision
      md5 = hashlib.md5()
      with open(archive_name, "rb") as f:
        for chunk in iter(lambda: f.read(8192), ""):
          md5.update(chunk)
      if md5.hexdigest() != TEST_262_ARCHIVE_MD5:
        os.remove(archive_name)
        raise Exception("Hash mismatch of test data file")
      archive = tarfile.open(archive_name, "r:gz")
      if sys.platform in ("win32", "cygwin"):
        # Magic incantation to allow longer path names on Windows.
        archive.extractall(u"\\\\?\\%s" % self.root)
      else:
        archive.extractall(self.root)
      os.rename(os.path.join(self.root, "tc39-test262-%s" % revision),
                directory_name)


def GetSuite(name, root):
  return Test262TestSuite(name, root)
