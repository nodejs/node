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
import itertools
import os
import re
import sys
import tarfile


from testrunner.local import statusfile
from testrunner.local import testsuite
from testrunner.local import utils
from testrunner.objects import testcase

# TODO(littledan): move the flag mapping into the status file
FEATURE_FLAGS = {
  'object-rest': '--harmony-object-rest-spread',
  'object-spread': '--harmony-object-rest-spread',
  'async-iteration': '--harmony-async-iteration',
}

DATA = os.path.join(os.path.dirname(os.path.abspath(__file__)), "data")
ARCHIVE = DATA + ".tar"

TEST_262_HARNESS_FILES = ["sta.js", "assert.js"]
TEST_262_NATIVE_FILES = ["detachArrayBuffer.js"]

TEST_262_SUITE_PATH = ["data", "test"]
TEST_262_HARNESS_PATH = ["data", "harness"]
TEST_262_TOOLS_PATH = ["harness", "src"]
TEST_262_LOCAL_TESTS_PATH = ["local-tests", "test"]

TEST_262_RELPATH_REGEXP = re.compile(
    r'.*[\\/]test[\\/]test262[\\/][^\\/]+[\\/]test[\\/](.*)\.js')

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
  # Match the (...) in '/path/to/v8/test/test262/subdir/test/(...).js'
  # In practice, subdir is data or local-tests

  def __init__(self, name, root):
    super(Test262TestSuite, self).__init__(name, root)
    self.testroot = os.path.join(self.root, *TEST_262_SUITE_PATH)
    self.harnesspath = os.path.join(self.root, *TEST_262_HARNESS_PATH)
    self.harness = [os.path.join(self.harnesspath, f)
                    for f in TEST_262_HARNESS_FILES]
    self.harness += [os.path.join(self.root, "harness-adapt.js")]
    self.localtestroot = os.path.join(self.root, *TEST_262_LOCAL_TESTS_PATH)
    self.ParseTestRecord = None

  def ListTests(self, context):
    tests = []
    testnames = set()
    for dirname, dirs, files in itertools.chain(os.walk(self.testroot),
                                                os.walk(self.localtestroot)):
      for dotted in [x for x in dirs if x.startswith(".")]:
        dirs.remove(dotted)
      if context.noi18n and "intl402" in dirs:
        dirs.remove("intl402")
      dirs.sort()
      files.sort()
      for filename in files:
        if not filename.endswith(".js"):
          continue
        if filename.endswith("_FIXTURE.js"):
          continue
        fullpath = os.path.join(dirname, filename)
        relpath = re.match(TEST_262_RELPATH_REGEXP, fullpath).group(1)
        testnames.add(relpath.replace(os.path.sep, "/"))
    return [testcase.TestCase(self, testname) for testname in testnames]

  def GetFlagsForTestCase(self, testcase, context):
    return (testcase.flags + context.mode_flags + self.harness +
            ([os.path.join(self.root, "harness-agent.js")]
             if testcase.path.startswith('built-ins/Atomics') else []) +
            self.GetIncludesForTest(testcase) +
            (["--module"] if "module" in self.GetTestRecord(testcase) else []) +
            [self.GetPathForTest(testcase)] +
            (["--throws"] if "negative" in self.GetTestRecord(testcase)
                          else []) +
            (["--allow-natives-syntax"]
             if "detachArrayBuffer.js" in
                self.GetTestRecord(testcase).get("includes", [])
             else []) +
            ([flag for flag in testcase.outcomes if flag.startswith("--")]) +
            ([flag for (feature, flag) in FEATURE_FLAGS.items()
              if feature in self.GetTestRecord(testcase).get("features", [])]))

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

  def GetPathForTest(self, testcase):
    filename = os.path.join(self.localtestroot, testcase.path + ".js")
    if not os.path.exists(filename):
      filename = os.path.join(self.testroot, testcase.path + ".js")
    return filename

  def GetSourceForTest(self, testcase):
    with open(self.GetPathForTest(testcase)) as f:
      return f.read()

  def _ParseException(self, str, testcase):
    # somefile:somelinenumber: someerror[: sometext]
    # somefile might include an optional drive letter on windows e.g. "e:".
    match = re.search(
        '^(?:\w:)?[^:]*:[0-9]+: ([^: ]+?)($|: )', str, re.MULTILINE)
    if match:
      return match.group(1).strip()
    else:
      print "Error parsing exception for %s" % testcase.GetLabel()
      return None

  def IsFailureOutput(self, testcase):
    output = testcase.output
    test_record = self.GetTestRecord(testcase)
    if output.exit_code != 0:
      return True
    if ("negative" in test_record and
        "type" in test_record["negative"] and
        self._ParseException(output.stdout, testcase) !=
            test_record["negative"]["type"]):
        return True
    return "FAILED!" in output.stdout

  def HasUnexpectedOutput(self, testcase):
    outcome = self.GetOutcome(testcase)
    if (statusfile.FAIL_SLOPPY in testcase.outcomes and
        "--use-strict" not in testcase.flags):
      return outcome != statusfile.FAIL
    return not outcome in ([outcome for outcome in testcase.outcomes
                                    if not outcome.startswith('--')
                                       and outcome != statusfile.FAIL_SLOPPY]
                           or [statusfile.PASS])

  def PrepareSources(self):
    # The archive is created only on swarming. Local checkouts have the
    # data folder.
    if (os.path.exists(ARCHIVE) and
        # Check for a JS file from the archive if we need to unpack. Some other
        # files from the archive unfortunately exist due to a bug in the
        # isolate_processor.
        # TODO(machenbach): Migrate this to GN to avoid using the faulty
        # isolate_processor: http://crbug.com/669910
        not os.path.exists(os.path.join(DATA, 'test', 'harness', 'error.js'))):
      print "Extracting archive..."
      tar = tarfile.open(ARCHIVE)
      tar.extractall(path=os.path.dirname(ARCHIVE))
      tar.close()


def GetSuite(name, root):
  return Test262TestSuite(name, root)
