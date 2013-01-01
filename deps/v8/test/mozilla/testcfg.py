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
    filename = join(self.testroot, testcase.path + ".js")
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
    os.rename(join("mozilla", "js", "tests"), directory_name)
    shutil.rmtree("mozilla")
    with tarfile.open(archive_file, "w:gz") as tar:
      tar.add("data")
    with open(versionfile, "w") as f:
      f.write(MOZILLA_VERSION)
    os.chdir(old_cwd)


def GetSuite(name, root):
  return MozillaTestSuite(name, root)


# Deprecated definitions below.
# TODO(jkummerow): Remove when SCons is no longer supported.


from os.path import exists
from os.path import join
import test


class MozillaTestCase(test.TestCase):

  def __init__(self, filename, path, context, root, mode, framework):
    super(MozillaTestCase, self).__init__(context, path, mode)
    self.filename = filename
    self.framework = framework
    self.root = root

  def IsNegative(self):
    return self.filename.endswith('-n.js')

  def GetLabel(self):
    return "%s mozilla %s" % (self.mode, self.GetName())

  def IsFailureOutput(self, output):
    if output.exit_code != 0:
      return True
    return 'FAILED!' in output.stdout

  def GetCommand(self):
    result = self.context.GetVmCommand(self, self.mode) + \
       [ '--expose-gc', join(self.root, 'mozilla-shell-emulation.js') ]
    result += [ '--es5_readonly' ]  # Temporary hack until we can remove flag
    result += self.framework
    result.append(self.filename)
    return result

  def GetName(self):
    return self.path[-1]

  def GetSource(self):
    return open(self.filename).read()


class MozillaTestConfiguration(test.TestConfiguration):

  def __init__(self, context, root):
    super(MozillaTestConfiguration, self).__init__(context, root)

  def ListTests(self, current_path, path, mode, variant_flags):
    tests = []
    for test_dir in TEST_DIRS:
      current_root = join(self.root, 'data', test_dir)
      for root, dirs, files in os.walk(current_root):
        for dotted in [x  for x in dirs if x.startswith('.')]:
          dirs.remove(dotted)
        for excluded in EXCLUDED:
          if excluded in dirs:
            dirs.remove(excluded)
        dirs.sort()
        root_path = root[len(self.root):].split(os.path.sep)
        root_path = current_path + [x for x in root_path if x]
        framework = []
        for i in xrange(len(root_path)):
          if i == 0: dir = root_path[1:]
          else: dir = root_path[1:-i]
          script = join(self.root, reduce(join, dir, ''), 'shell.js')
          if exists(script):
            framework.append(script)
        framework.reverse()
        files.sort()
        for file in files:
          if (not file in FRAMEWORK) and file.endswith('.js'):
            full_path = root_path + [file[:-3]]
            full_path = [x for x in full_path if x != 'data']
            if self.Contains(path, full_path):
              test = MozillaTestCase(join(root, file), full_path, self.context,
                                     self.root, mode, framework)
              tests.append(test)
    return tests

  def GetBuildRequirements(self):
    return ['d8']

  def GetTestStatus(self, sections, defs):
    status_file = join(self.root, 'mozilla.status')
    if exists(status_file):
      test.ReadConfigurationInto(status_file, sections, defs)


def GetConfiguration(context, root):
  return MozillaTestConfiguration(context, root)
