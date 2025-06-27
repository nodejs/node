# Copyright 2018 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from . import base


STANDARD_VARIANT = set(["default"])


class VariantProc(base.TestProcProducer):
  """Processor creating variants.

  For each test it keeps generator that returns variant, flags and id suffix.
  It produces variants one at a time, so it's waiting for the result of one
  variant to create another variant of the same test.
  It maintains the order of the variants passed to the init.

  There are some cases when particular variant of the test is not valid. To
  ignore subtests like that, StatusFileFilterProc should be placed somewhere
  after the VariantProc.
  """

  def __init__(self, variants):
    super(VariantProc, self).__init__('VariantProc')
    self._requirement = base.DROP_RESULT
    self._next_variant = {}
    self._variant_gens = {}
    self._variants = variants

  def test_suffix(self, test):
    return test.variant

  def _next_test(self, test):
    gen = self._variants_gen(test)
    self._next_variant[test.procid] = gen
    return self._try_send_new_subtest(test, gen)

  def _result_for(self, test, subtest, result):
    # The generator might have been removed after cycling through all subtests
    # below. If some of the subtests are heavy, they get buffered and return
    # their results later.
    gen = self._next_variant.get(test.procid)
    if not gen or not self._try_send_new_subtest(test, gen):
      self._send_result(test, None)

  def _try_send_new_subtest(self, test, variants_gen):
    for variant, flags, suffix in variants_gen:
      subtest = test.create_subtest(
          self, '%s-%s' % (variant, suffix), variant=variant, flags=flags)
      if self._send_test(subtest):
        return True

    del self._next_variant[test.procid]
    return False

  def _variants_gen(self, test):
    """Generator producing (variant, flags, procid suffix) tuples."""
    return self._get_variants_gen(test).gen(test)

  def _get_variants_gen(self, test):
    key = test.suite.name
    variants_gen = self._variant_gens.get(key)
    if not variants_gen:
      variants_gen = test.suite.get_variants_gen(self._variants)
      self._variant_gens[key] = variants_gen
    return variants_gen
