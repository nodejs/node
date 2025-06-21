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


class TestLoader(testsuite.TestLoader):
  def _list_test_filenames(self):
    return [
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
        "sunspider/string-validate-input",
    ]


class TestSuite(testsuite.TestSuite):

  def __init__(self, ctx, *args, **kwargs):
    super(TestSuite, self).__init__(ctx, *args, **kwargs)
    self.testroot = self.root / "data"

  def _test_loader_class(self):
    return TestLoader

  def _test_class(self):
    return TestCase


class TestCase(testcase.D8TestCase):
  def _get_files_params(self):
    path = self.path
    testroot = self.suite.testroot
    files = []
    if path.parts[0] == "kraken":
      files.append(testroot / f"{path}-data.js")
      files.append(testroot / f"{path}.js")
    elif path.parts[0] == "octane":
      files.append(testroot / "octane/base.js")
      files.append(testroot / f"{path}.js")
      if path.parts[1] == "gbemu":
        files.append(testroot / "octane/gbemu-part2.js")
      elif path.parts[1] == "typescript":
        files.append(testroot / "octane/typescript-compiler.js")
        files.append(testroot / "octane/typescript-input.js")
      elif path.parts[1] == "zlib":
        files.append(testroot / "octane/zlib-data.js")
      files += ["-e", "BenchmarkSuite.RunSuites({});"]
    elif path.parts[0] == "sunspider":
      files.append(self._get_source_path())
    return files

  def _get_source_path(self):
    return self.suite.testroot / self.path_js
