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

SIMDJS_ARCHIVE_REVISION = "99ef44bd4f22acd203c01e524131bc7f2a7eab68"
SIMDJS_ARCHIVE_MD5 = "1428773887924fa5a784bf0843615740"
SIMDJS_URL = ("https://github.com/tc39/ecmascript_simd/archive/%s.tar.gz")

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

    archive_prefix = "ecmascript_simd-"
    archive_name = os.path.join(
        self.root, "%s%s.tar.gz" % (archive_prefix, revision))
    directory_name = os.path.join(self.root, "data")
    directory_old_name = os.path.join(self.root, "data.old")
    versionfile = os.path.join(self.root, "CHECKED_OUT_VERSION")

    checked_out_version = None
    checked_out_url = None
    checked_out_revision = None
    if os.path.exists(versionfile):
      with open(versionfile) as f:
        try:
          (checked_out_version,
           checked_out_url,
           checked_out_revision) = f.read().splitlines()
        except ValueError:
          pass
    if (checked_out_version != SIMDJS_ARCHIVE_MD5 or
        checked_out_url != archive_url or
        checked_out_revision != revision):
      if os.path.exists(archive_name):
        print "Clobbering %s because CHECK_OUT_VERSION is out of date" % (
            archive_name)
        os.remove(archive_name)

    # Clobber if the test is in an outdated state, i.e. if there are any other
    # archive files present.
    archive_files = [f for f in os.listdir(self.root)
                     if f.startswith(archive_prefix)]
    if (len(archive_files) > 1 or
        os.path.basename(archive_name) not in archive_files):
      print "Clobber outdated test archives ..."
      for f in archive_files:
        print "Removing %s" % f
        os.remove(os.path.join(self.root, f))

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

      with open(versionfile, "w") as f:
        f.write(SIMDJS_ARCHIVE_MD5 + '\n')
        f.write(archive_url + '\n')
        f.write(revision + '\n')


def GetSuite(name, root):
  return SimdJsTestSuite(name, root)
