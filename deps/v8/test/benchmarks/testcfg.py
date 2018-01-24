# Copyright 2013 the V8 project authors. All rights reserved.
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

from testrunner.local import statusfile
from testrunner.local import testsuite
from testrunner.objects import testcase


class BenchmarksVariantGenerator(testsuite.VariantGenerator):
  # Both --noopt and --stressopt are very slow. Add TF but without
  # always opt to match the way the benchmarks are run for performance
  # testing.
  def FilterVariantsByTest(self, testcase):
    outcomes = self.suite.GetStatusFileOutcomes(testcase)
    if statusfile.OnlyStandardVariant(outcomes):
      return self.standard_variant
    return self.fast_variants

  def GetFlagSets(self, testcase, variant):
    return testsuite.FAST_VARIANT_FLAGS[variant]


class BenchmarksTestSuite(testsuite.TestSuite):

  def __init__(self, name, root):
    super(BenchmarksTestSuite, self).__init__(name, root)
    self.testroot = os.path.join(root, "data")

  def ListTests(self, context):
    tests = []
    for test in [
        "kraken/ai-astar",
        "kraken/audio-beat-detection",
        "kraken/audio-dft",
        "kraken/audio-fft",
        "kraken/audio-oscillator",
        "kraken/imaging-darkroom",
        "kraken/imaging-desaturate",
        "kraken/imaging-gaussian-blur",
        "kraken/json-parse-financial",
        "kraken/json-stringify-tinderbox",
        "kraken/stanford-crypto-aes",
        "kraken/stanford-crypto-ccm",
        "kraken/stanford-crypto-pbkdf2",
        "kraken/stanford-crypto-sha256-iterative",

        "octane/box2d",
        "octane/code-load",
        "octane/crypto",
        "octane/deltablue",
        "octane/earley-boyer",
        "octane/gbemu-part1",
        "octane/mandreel",
        "octane/navier-stokes",
        "octane/pdfjs",
        "octane/raytrace",
        "octane/regexp",
        "octane/richards",
        "octane/splay",
        "octane/typescript",
        "octane/zlib",

        "sunspider/3d-cube",
        "sunspider/3d-morph",
        "sunspider/3d-raytrace",
        "sunspider/access-binary-trees",
        "sunspider/access-fannkuch",
        "sunspider/access-nbody",
        "sunspider/access-nsieve",
        "sunspider/bitops-3bit-bits-in-byte",
        "sunspider/bitops-bits-in-byte",
        "sunspider/bitops-bitwise-and",
        "sunspider/bitops-nsieve-bits",
        "sunspider/controlflow-recursive",
        "sunspider/crypto-aes",
        "sunspider/crypto-md5",
        "sunspider/crypto-sha1",
        "sunspider/date-format-tofte",
        "sunspider/date-format-xparb",
        "sunspider/math-cordic",
        "sunspider/math-partial-sums",
        "sunspider/math-spectral-norm",
        "sunspider/regexp-dna",
        "sunspider/string-base64",
        "sunspider/string-fasta",
        "sunspider/string-tagcloud",
        "sunspider/string-unpack-code",
        "sunspider/string-validate-input"]:
      tests.append(testcase.TestCase(self, test))
    return tests

  def GetParametersForTestCase(self, testcase, context):
    files = []
    if testcase.path.startswith("kraken"):
      files.append(os.path.join(self.testroot, "%s-data.js" % testcase.path))
      files.append(os.path.join(self.testroot, "%s.js" % testcase.path))
    elif testcase.path.startswith("octane"):
      files.append(os.path.join(self.testroot, "octane/base.js"))
      files.append(os.path.join(self.testroot, "%s.js" % testcase.path))
      if testcase.path.startswith("octane/gbemu"):
        files.append(os.path.join(self.testroot, "octane/gbemu-part2.js"))
      elif testcase.path.startswith("octane/typescript"):
        files.append(os.path.join(self.testroot,
                                  "octane/typescript-compiler.js"))
        files.append(os.path.join(self.testroot, "octane/typescript-input.js"))
      elif testcase.path.startswith("octane/zlib"):
        files.append(os.path.join(self.testroot, "octane/zlib-data.js"))
      files += ["-e", "BenchmarkSuite.RunSuites({});"]
    elif testcase.path.startswith("sunspider"):
      files.append(os.path.join(self.testroot, "%s.js" % testcase.path))

    return files, testcase.flags + context.mode_flags, {}

  def GetSourceForTest(self, testcase):
    filename = os.path.join(self.testroot, testcase.path + ".js")
    with open(filename) as f:
      return f.read()

  def _VariantGeneratorFactory(self):
    return BenchmarksVariantGenerator


def GetSuite(name, root):
  return BenchmarksTestSuite(name, root)
