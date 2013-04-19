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


import os
import shutil
import subprocess
import tarfile

from testrunner.local import testsuite
from testrunner.objects import testcase


MOZILLA_VERSION = "2010-06-29"


EXCLUDED = ["CVS"]


FRAMEWORK = """
  browser.js
  shell.js
  jsref.js
  template.js
""".split()


TEST_DIRS = """
  ecma
  ecma_2
  ecma_3
  js1_1
  js1_2
  js1_3
  js1_4
  js1_5
""".split()


class MozillaTestSuite(testsuite.TestSuite):

  def __init__(self, name, root):
    super(MozillaTestSuite, self).__init__(name, root)
    self.testroot = os.path.join(root, "data")

  def ListTests(self, context):
    tests = []
    for testdir in TEST_DIRS:
      current_root = os.path.join(self.testroot, testdir)
      for dirname, dirs, files in os.walk(current_root):
        for dotted in [x for x in dirs if x.startswith(".")]:
          dirs.remove(dotted)
        for excluded in EXCLUDED:
          if excluded in dirs:
            dirs.remove(excluded)
        dirs.sort()
        files.sort()
        for filename in files:
          if filename.endswith(".js") and not filename in FRAMEWORK:
            testname = os.path.join(dirname[len(self.testroot) + 1:],
                                    filename[:-3])
            case = testcase.TestCase(self, testname)
            tests.append(case)
    return tests

  def GetFlagsForTestCase(self, testcase, context):
    result = []
    result += context.mode_flags
    result += ["--expose-gc"]
    result += [os.path.join(self.root, "mozilla-shell-emulation.js")]
    testfilename = testcase.path + ".js"
    testfilepath = testfilename.split(os.path.sep)
    for i in xrange(len(testfilepath)):
      script = os.path.join(self.testroot,
                            reduce(os.path.join, testfilepath[:i], ""),
                            "shell.js")
      if os.path.exists(script):
        result.append(script)
    result.append(os.path.join(self.testroot, testfilename))
    return testcase.flags + result

  def GetSourceForTest(self, testcase):
    filename = os.path.join(self.testroot, testcase.path + ".js")
    with open(filename) as f:
      return f.read()

  def IsNegativeTest(self, testcase):
    return testcase.path.endswith("-n")

  def IsFailureOutput(self, output, testpath):
    if output.exit_code != 0:
      return True
    return "FAILED!" in output.stdout

  def DownloadData(self):
    old_cwd = os.getcwd()
    os.chdir(os.path.abspath(self.root))

    # Maybe we're still up to date?
    versionfile = "CHECKED_OUT_VERSION"
    checked_out_version = None
    if os.path.exists(versionfile):
      with open(versionfile) as f:
        checked_out_version = f.read()
    if checked_out_version == MOZILLA_VERSION:
      os.chdir(old_cwd)
      return

    # If we have a local archive file with the test data, extract it.
    directory_name = "data"
    if os.path.exists(directory_name):
      os.rename(directory_name, "data.old")
    archive_file = "downloaded_%s.tar.gz" % MOZILLA_VERSION
    if os.path.exists(archive_file):
      with tarfile.open(archive_file, "r:gz") as tar:
        tar.extractall()
      with open(versionfile, "w") as f:
        f.write(MOZILLA_VERSION)
      os.chdir(old_cwd)
      return

    # No cached copy. Check out via CVS, and pack as .tar.gz for later use.
    command = ("cvs -d :pserver:anonymous@cvs-mirror.mozilla.org:/cvsroot"
               " co -D %s mozilla/js/tests" % MOZILLA_VERSION)
    code = subprocess.call(command, shell=True)
    if code != 0:
      os.chdir(old_cwd)
      raise Exception("Error checking out Mozilla test suite!")
    os.rename(os.path.join("mozilla", "js", "tests"), directory_name)
    shutil.rmtree("mozilla")
    with tarfile.open(archive_file, "w:gz") as tar:
      tar.add("data")
    with open(versionfile, "w") as f:
      f.write(MOZILLA_VERSION)
    os.chdir(old_cwd)


def GetSuite(name, root):
  return MozillaTestSuite(name, root)
