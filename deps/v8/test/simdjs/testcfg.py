# Copyright 2014 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


import hashlib
import os
import shutil
import sys
import tarfile
import imp

from testrunner.local import testsuite
from testrunner.local import utils
from testrunner.objects import testcase

SIMDJS_ARCHIVE_REVISION = "07e2713e0c9ea19feb0732d5bd84770c87310d79"
SIMDJS_ARCHIVE_MD5 = "cf6bddf99f18800b68e782054268ee3c"
SIMDJS_URL = (
    "https://github.com/johnmccutchan/ecmascript_simd/archive/%s.tar.gz")

SIMDJS_SUITE_PATH = ["data", "src"]


class SimdJsTestSuite(testsuite.TestSuite):

  def __init__(self, name, root):
    super(SimdJsTestSuite, self).__init__(name, root)
    self.testroot = os.path.join(self.root, *SIMDJS_SUITE_PATH)
    self.ParseTestRecord = None

  def ListTests(self, context):
    tests = [
      testcase.TestCase(self, 'shell_test_runner'),
    ]
    for filename in os.listdir(os.path.join(self.testroot, 'benchmarks')):
      if (not filename.endswith('.js') or
          filename in ['run.js', 'run_browser.js', 'base.js']):
        continue
      name = filename.rsplit('.')[0]
      tests.append(
          testcase.TestCase(self, 'benchmarks/' + name))
    return tests

  def GetFlagsForTestCase(self, testcase, context):
    return (testcase.flags + context.mode_flags +
            [os.path.join(self.root, "harness-adapt.js"),
             "--harmony",
             os.path.join(self.testroot, testcase.path + ".js"),
             os.path.join(self.root, "harness-finish.js")])

  def GetSourceForTest(self, testcase):
    filename = os.path.join(self.testroot, testcase.path + ".js")
    with open(filename) as f:
      return f.read()

  def IsNegativeTest(self, testcase):
    return False

  def IsFailureOutput(self, output, testpath):
    if output.exit_code != 0:
      return True
    return "FAILED!" in output.stdout

  def DownloadData(self):
    revision = SIMDJS_ARCHIVE_REVISION
    archive_url = SIMDJS_URL % revision
    archive_name = os.path.join(
        self.root, "ecmascript_simd-%s.tar.gz" % revision)
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
      print "Extracting ecmascript_simd-%s.tar.gz ..." % revision
      md5 = hashlib.md5()
      with open(archive_name, "rb") as f:
        for chunk in iter(lambda: f.read(8192), ""):
          md5.update(chunk)
      print "MD5 hash is %s" % md5.hexdigest()
      if md5.hexdigest() != SIMDJS_ARCHIVE_MD5:
        os.remove(archive_name)
        print "MD5 expected %s" % SIMDJS_ARCHIVE_MD5
        raise Exception("MD5 hash mismatch of test data file")
      archive = tarfile.open(archive_name, "r:gz")
      if sys.platform in ("win32", "cygwin"):
        # Magic incantation to allow longer path names on Windows.
        archive.extractall(u"\\\\?\\%s" % self.root)
      else:
        archive.extractall(self.root)
      os.rename(os.path.join(self.root, "ecmascript_simd-%s" % revision),
                directory_name)


def GetSuite(name, root):
  return SimdJsTestSuite(name, root)
