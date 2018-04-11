# Copyright 2011 the V8 project authors. All rights reserved.
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

from testrunner.local import testsuite
from testrunner.objects import testcase


class VariantsGenerator(testsuite.VariantsGenerator):
  def _get_variants(self, test):
    return self._standard_variant


class TestSuite(testsuite.TestSuite):
  def _ParsePythonTestTemplates(self, result, filename):
    pathname = os.path.join(self.root, filename + ".pyt")
    def Test(name, source, expectation):
      source = source.replace("\n", " ")
      path = os.path.join(filename, name)
      if expectation:
        template_flags = ["--throws"]
      else:
        template_flags = []
      test = self._create_test(path, source, template_flags)
      result.append(test)
    def Template(name, source):
      def MkTest(replacement, expectation):
        testname = name
        testsource = source
        for key in replacement.keys():
          testname = testname.replace("$" + key, replacement[key]);
          testsource = testsource.replace("$" + key, replacement[key]);
        Test(testname, testsource, expectation)
      return MkTest
    execfile(pathname, {"Test": Test, "Template": Template})

  def ListTests(self):
    result = []

    # Find all .pyt files in this directory.
    filenames = [f[:-4] for f in os.listdir(self.root) if f.endswith(".pyt")]
    filenames.sort()
    for f in filenames:
      self._ParsePythonTestTemplates(result, f)
    return result

  def _create_test(self, path, source, template_flags):
    return super(TestSuite, self)._create_test(
        path, source=source, template_flags=template_flags)

  def _test_class(self):
    return TestCase

  def _variants_gen_class(self):
    return VariantsGenerator


class TestCase(testcase.TestCase):
  def __init__(self, suite, path, name, test_config, source, template_flags):
    super(TestCase, self).__init__(suite, path, name, test_config)

    self._source = source
    self._template_flags = template_flags

  def _get_cmd_params(self):
    return (
        self._get_files_params() +
        self._get_extra_flags() +
        ['-e', self._source] +
        self._template_flags +
        self._get_variant_flags() +
        self._get_statusfile_flags() +
        self._get_mode_flags() +
        self._get_source_flags()
    )

  def _get_mode_flags(self):
    return []

  def is_source_available(self):
    return True

  def get_source(self):
    return self._source


def GetSuite(*args, **kwargs):
  return TestSuite(*args, **kwargs)
