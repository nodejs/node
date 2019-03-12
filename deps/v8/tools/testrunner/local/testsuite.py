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


import fnmatch
import imp
import os
from contextlib import contextmanager

from . import command
from . import statusfile
from . import utils
from ..objects.testcase import TestCase
from .variants import ALL_VARIANTS, ALL_VARIANT_FLAGS


STANDARD_VARIANT = set(["default"])


class VariantsGenerator(object):
  def __init__(self, variants):
    self._all_variants = [v for v in variants if v in ALL_VARIANTS]
    self._standard_variant = [v for v in variants if v in STANDARD_VARIANT]

  def gen(self, test):
    """Generator producing (variant, flags, procid suffix) tuples."""
    flags_set = self._get_flags_set(test)
    for n, variant in enumerate(self._get_variants(test)):
      yield (variant, flags_set[variant][0], n)

  def _get_flags_set(self, test):
    return ALL_VARIANT_FLAGS

  def _get_variants(self, test):
    if test.only_standard_variant:
      return self._standard_variant
    return self._all_variants


class TestCombiner(object):
  def get_group_key(self, test):
    """To indicate what tests can be combined with each other we define a group
    key for each test. Tests with the same group key can be combined. Test
    without a group key (None) is not combinable with any other test.
    """
    raise NotImplementedError()

  def combine(self, name, tests):
    """Returns test combined from `tests`. Since we identify tests by their
    suite and name, `name` parameter should be unique within one suite.
    """
    return self._combined_test_class()(name, tests)

  def _combined_test_class(self):
    raise NotImplementedError()

@contextmanager
def _load_testsuite_module(name, root):
  f = None
  try:
    (f, pathname, description) = imp.find_module("testcfg", [root])
    yield imp.load_module(name + "_testcfg", f, pathname, description)
  finally:
    if f:
      f.close()

class TestSuite(object):
  @staticmethod
  def Load(root, test_config):
    name = root.split(os.path.sep)[-1]
    with _load_testsuite_module(name, root) as module:
      return module.GetSuite(name, root, test_config)

  def __init__(self, name, root, test_config):
    self.name = name  # string
    self.root = root  # string containing path
    self.test_config = test_config
    self.tests = None  # list of TestCase objects
    self.statusfile = None

  def status_file(self):
    return "%s/%s.status" % (self.root, self.name)

  def ListTests(self):
    raise NotImplementedError

  def load_tests_from_disk(self, statusfile_variables):
    self.statusfile = statusfile.StatusFile(
      self.status_file(), statusfile_variables)

    slow_tests = (test for test in self.ListTests() if test.is_slow)
    fast_tests = (test for test in self.ListTests() if not test.is_slow)

    return slow_tests, fast_tests

  def get_variants_gen(self, variants):
    return self._variants_gen_class()(variants)

  def _variants_gen_class(self):
    return VariantsGenerator

  def test_combiner_available(self):
    return bool(self._test_combiner_class())

  def get_test_combiner(self):
    cls = self._test_combiner_class()
    if cls:
      return cls()
    return None

  def _test_combiner_class(self):
    """Returns Combiner subclass. None if suite doesn't support combining
    tests.
    """
    return None

  def _create_test(self, path, **kwargs):
    test_class = self._test_class()
    return test_class(self, path, self._path_to_name(path), self.test_config,
                      **kwargs)

  def _test_class(self):
    raise NotImplementedError

  def _path_to_name(self, path):
    if utils.IsWindows():
      return path.replace("\\", "/")
    return path
