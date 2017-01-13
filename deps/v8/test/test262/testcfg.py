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


import imp
import os
import sys
import tarfile


from testrunner.local import statusfile
from testrunner.local import testsuite
from testrunner.local import utils
from testrunner.objects import testcase

DATA = os.path.join(os.path.dirname(os.path.abspath(__file__)), "data")
ARCHIVE = DATA + ".tar"

TEST_262_HARNESS_FILES = ["sta.js", "assert.js"]
TEST_262_NATIVE_FILES = ["detachArrayBuffer.js"]

TEST_262_SUITE_PATH = ["data", "test"]
TEST_262_HARNESS_PATH = ["data", "harness"]
TEST_262_TOOLS_PATH = ["harness", "src"]

sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                             *TEST_262_TOOLS_PATH))

ALL_VARIANT_FLAGS_STRICT = dict(
    (v, [flags + ["--use-strict"] for flags in flag_sets])
    for v, flag_sets in testsuite.ALL_VARIANT_FLAGS.iteritems()
)

FAST_VARIANT_FLAGS_STRICT = dict(
    (v, [flags + ["--use-strict"] for flags in flag_sets])
    for v, flag_sets in testsuite.FAST_VARIANT_FLAGS.iteritems()
)

ALL_VARIANT_FLAGS_BOTH = dict(
    (v, [flags for flags in testsuite.ALL_VARIANT_FLAGS[v] +
                            ALL_VARIANT_FLAGS_STRICT[v]])
    for v in testsuite.ALL_VARIANT_FLAGS
)

FAST_VARIANT_FLAGS_BOTH = dict(
    (v, [flags for flags in testsuite.FAST_VARIANT_FLAGS[v] +
                            FAST_VARIANT_FLAGS_STRICT[v]])
    for v in testsuite.FAST_VARIANT_FLAGS
)

ALL_VARIANTS = {
  'nostrict': testsuite.ALL_VARIANT_FLAGS,
  'strict': ALL_VARIANT_FLAGS_STRICT,
  'both': ALL_VARIANT_FLAGS_BOTH,
}

FAST_VARIANTS = {
  'nostrict': testsuite.FAST_VARIANT_FLAGS,
  'strict': FAST_VARIANT_FLAGS_STRICT,
  'both': FAST_VARIANT_FLAGS_BOTH,
}

class Test262VariantGenerator(testsuite.VariantGenerator):
  def GetFlagSets(self, testcase, variant):
    if testcase.outcomes and statusfile.OnlyFastVariants(testcase.outcomes):
      variant_flags = FAST_VARIANTS
    else:
      variant_flags = ALL_VARIANTS

    test_record = self.suite.GetTestRecord(testcase)
    if "noStrict" in test_record:
      return variant_flags["nostrict"][variant]
    if "onlyStrict" in test_record:
      return variant_flags["strict"][variant]
    return variant_flags["both"][variant]


class Test262TestSuite(testsuite.TestSuite):

  def __init__(self, name, root):
    super(Test262TestSuite, self).__init__(name, root)
    self.testroot = os.path.join(self.root, *TEST_262_SUITE_PATH)
    self.harnesspath = os.path.join(self.root, *TEST_262_HARNESS_PATH)
    self.harness = [os.path.join(self.harnesspath, f)
                    for f in TEST_262_HARNESS_FILES]
    self.harness += [os.path.join(self.root, "harness-adapt.js")]
    self.ParseTestRecord = None

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
          fullpath = os.path.join(dirname, filename)
          relpath = fullpath[len(self.testroot) + 1 : -3]
          testname = relpath.replace(os.path.sep, "/")
          case = testcase.TestCase(self, testname)
          tests.append(case)
    return tests

  def GetFlagsForTestCase(self, testcase, context):
    return (testcase.flags + context.mode_flags + self.harness +
            self.GetIncludesForTest(testcase) + ["--harmony"] +
            (["--module"] if "module" in self.GetTestRecord(testcase) else []) +
            [os.path.join(self.testroot, testcase.path + ".js")] +
            (["--throws"] if "negative" in self.GetTestRecord(testcase)
                          else []) +
            (["--allow-natives-syntax"]
             if "detachArrayBuffer.js" in
                self.GetTestRecord(testcase).get("includes", [])
             else []))

  def _VariantGeneratorFactory(self):
    return Test262VariantGenerator

  def LoadParseTestRecord(self):
    if not self.ParseTestRecord:
      root = os.path.join(self.root, *TEST_262_TOOLS_PATH)
      f = None
      try:
        (f, pathname, description) = imp.find_module("parseTestRecord", [root])
        module = imp.load_module("parseTestRecord", f, pathname, description)
        self.ParseTestRecord = module.parseTestRecord
      except:
        raise ImportError("Cannot load parseTestRecord; you may need to "
                          "gclient sync for test262")
      finally:
        if f:
          f.close()
    return self.ParseTestRecord

  def GetTestRecord(self, testcase):
    if not hasattr(testcase, "test_record"):
      ParseTestRecord = self.LoadParseTestRecord()
      testcase.test_record = ParseTestRecord(self.GetSourceForTest(testcase),
                                             testcase.path)
    return testcase.test_record

  def BasePath(self, filename):
    return self.root if filename in TEST_262_NATIVE_FILES else self.harnesspath

  def GetIncludesForTest(self, testcase):
    test_record = self.GetTestRecord(testcase)
    if "includes" in test_record:
      return [os.path.join(self.BasePath(filename), filename)
              for filename in test_record.get("includes", [])]
    else:
      includes = []
    return includes

  def GetSourceForTest(self, testcase):
    filename = os.path.join(self.testroot, testcase.path + ".js")
    with open(filename) as f:
      return f.read()

  def _ParseException(self, str):
    for line in str.split("\n")[::-1]:
      if line and not line[0].isspace() and ":" in line:
        return line.split(":")[0]


  def IsFailureOutput(self, testcase):
    output = testcase.output
    test_record = self.GetTestRecord(testcase)
    if output.exit_code != 0:
      return True
    if "negative" in test_record:
      if self._ParseException(output.stdout) != test_record["negative"]:
        return True
    return "FAILED!" in output.stdout

  def HasUnexpectedOutput(self, testcase):
    outcome = self.GetOutcome(testcase)
    if (statusfile.FAIL_SLOPPY in testcase.outcomes and
        "--use-strict" not in testcase.flags):
      return outcome != statusfile.FAIL
    return not outcome in (testcase.outcomes or [statusfile.PASS])

  def PrepareSources(self):
    # The archive is created only on swarming. Local checkouts have the
    # data folder.
    if os.path.exists(ARCHIVE) and not os.path.exists(DATA):
      print "Extracting archive..."
      tar = tarfile.open(ARCHIVE)
      tar.extractall(path=os.path.dirname(ARCHIVE))
      tar.close()


def GetSuite(name, root):
  return Test262TestSuite(name, root)
