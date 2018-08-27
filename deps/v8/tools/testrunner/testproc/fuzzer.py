# Copyright 2018 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from collections import namedtuple
import time

from . import base


class FuzzerConfig(object):
  def __init__(self, probability, analyzer, fuzzer):
    """
    Args:
      probability: of choosing this fuzzer (0; 10]
      analyzer: instance of Analyzer class, can be None if no analysis is needed
      fuzzer: instance of Fuzzer class
    """
    assert probability > 0 and probability <= 10

    self.probability = probability
    self.analyzer = analyzer
    self.fuzzer = fuzzer


class Analyzer(object):
  def get_analysis_flags(self):
    raise NotImplementedError()

  def do_analysis(self, result):
    raise NotImplementedError()


class Fuzzer(object):
  def create_flags_generator(self, rng, test, analysis_value):
    """
    Args:
      rng: random number generator
      test: test for which to create flags
      analysis_value: value returned by the analyzer. None if there is no
        corresponding analyzer to this fuzzer or the analysis phase is disabled
    """
    raise NotImplementedError()


# TODO(majeski): Allow multiple subtests to run at once.
class FuzzerProc(base.TestProcProducer):
  def __init__(self, rng, count, fuzzers, disable_analysis=False):
    """
    Args:
      rng: random number generator used to select flags and values for them
      count: number of tests to generate based on each base test
      fuzzers: list of FuzzerConfig instances
      disable_analysis: disable analysis phase and filtering base on it. When
        set, processor passes None as analysis result to fuzzers
    """
    super(FuzzerProc, self).__init__('Fuzzer')

    self._rng = rng
    self._count = count
    self._fuzzer_configs = fuzzers
    self._disable_analysis = disable_analysis
    self._gens = {}

  def setup(self, requirement=base.DROP_RESULT):
    # Fuzzer is optimized to not store the results
    assert requirement == base.DROP_RESULT
    super(FuzzerProc, self).setup(requirement)

  def _next_test(self, test):
    if self.is_stopped:
      return

    analysis_subtest = self._create_analysis_subtest(test)
    if analysis_subtest:
      self._send_test(analysis_subtest)
    else:
      self._gens[test.procid] = self._create_gen(test)
      self._try_send_next_test(test)

  def _create_analysis_subtest(self, test):
    if self._disable_analysis:
      return None

    analysis_flags = []
    for fuzzer_config in self._fuzzer_configs:
      if fuzzer_config.analyzer:
        analysis_flags += fuzzer_config.analyzer.get_analysis_flags()

    if analysis_flags:
      analysis_flags = list(set(analysis_flags))
      return self._create_subtest(test, 'analysis', flags=analysis_flags,
                                  keep_output=True)


  def _result_for(self, test, subtest, result):
    if not self._disable_analysis:
      if result is not None:
        # Analysis phase, for fuzzing we drop the result.
        if result.has_unexpected_output:
          self._send_result(test, None)
          return
        self._gens[test.procid] = self._create_gen(test, result)

    self._try_send_next_test(test)

  def _create_gen(self, test, analysis_result=None):
    # It will be called with analysis_result==None only when there is no
    # analysis phase at all, so no fuzzer has it's own analyzer.
    gens = []
    indexes = []
    for i, fuzzer_config in enumerate(self._fuzzer_configs):
      analysis_value = None
      if analysis_result and fuzzer_config.analyzer:
        analysis_value = fuzzer_config.analyzer.do_analysis(analysis_result)
        if not analysis_value:
          # Skip fuzzer for this test since it doesn't have analysis data
          continue
      p = fuzzer_config.probability
      flag_gen = fuzzer_config.fuzzer.create_flags_generator(self._rng, test,
                                                             analysis_value)
      indexes += [len(gens)] * p
      gens.append((p, flag_gen))

    if not gens:
      # No fuzzers for this test, skip it
      return

    i = 0
    while not self._count or i < self._count:
      main_index = self._rng.choice(indexes)
      _, main_gen = gens[main_index]

      flags = next(main_gen)
      for index, (p, gen) in enumerate(gens):
        if index == main_index:
          continue
        if self._rng.randint(1, 10) <= p:
          flags += next(gen)

      flags.append('--fuzzer-random-seed=%s' % self._next_seed())
      yield self._create_subtest(test, str(i), flags=flags)

      i += 1

  def _try_send_next_test(self, test):
    if not self.is_stopped:
      for subtest in self._gens[test.procid]:
        self._send_test(subtest)
        return

    del self._gens[test.procid]
    self._send_result(test, None)

  def _next_seed(self):
    seed = None
    while not seed:
      seed = self._rng.randint(-2147483648, 2147483647)
    return seed


class ScavengeAnalyzer(Analyzer):
  def get_analysis_flags(self):
    return ['--fuzzer-gc-analysis']

  def do_analysis(self, result):
    for line in reversed(result.output.stdout.splitlines()):
      if line.startswith('### Maximum new space size reached = '):
        return int(float(line.split()[7]))


class ScavengeFuzzer(Fuzzer):
  def create_flags_generator(self, rng, test, analysis_value):
    while True:
      yield ['--stress-scavenge=%d' % (analysis_value or 100)]


class MarkingAnalyzer(Analyzer):
  def get_analysis_flags(self):
    return ['--fuzzer-gc-analysis']

  def do_analysis(self, result):
    for line in reversed(result.output.stdout.splitlines()):
      if line.startswith('### Maximum marking limit reached = '):
        return int(float(line.split()[6]))


class MarkingFuzzer(Fuzzer):
  def create_flags_generator(self, rng, test, analysis_value):
    while True:
      yield ['--stress-marking=%d' % (analysis_value or 100)]


class GcIntervalAnalyzer(Analyzer):
  def get_analysis_flags(self):
    return ['--fuzzer-gc-analysis']

  def do_analysis(self, result):
    for line in reversed(result.output.stdout.splitlines()):
      if line.startswith('### Allocations = '):
        return int(float(line.split()[3][:-1]))


class GcIntervalFuzzer(Fuzzer):
  def create_flags_generator(self, rng, test, analysis_value):
    if analysis_value:
      value = analysis_value / 10
    else:
      value = 10000
    while True:
      yield ['--random-gc-interval=%d' % value]


class CompactionFuzzer(Fuzzer):
  def create_flags_generator(self, rng, test, analysis_value):
    while True:
      yield ['--stress-compaction-random']


class ThreadPoolSizeFuzzer(Fuzzer):
  def create_flags_generator(self, rng, test, analysis_value):
    while True:
      yield ['--thread-pool-size=%d' % rng.randint(1, 8)]


class InterruptBudgetFuzzer(Fuzzer):
  def create_flags_generator(self, rng, test, analysis_value):
    while True:
      limit = 1 + int(rng.random() * 144)
      yield ['--interrupt-budget=%d' % rng.randint(1, limit * 1024)]


class DeoptAnalyzer(Analyzer):
  MAX_DEOPT=1000000000

  def __init__(self, min_interval):
    super(DeoptAnalyzer, self).__init__()
    self._min = min_interval

  def get_analysis_flags(self):
    return ['--deopt-every-n-times=%d' % self.MAX_DEOPT,
            '--print-deopt-stress']

  def do_analysis(self, result):
    for line in reversed(result.output.stdout.splitlines()):
      if line.startswith('=== Stress deopt counter: '):
        counter = self.MAX_DEOPT - int(line.split(' ')[-1])
        if counter < self._min:
          # Skip this test since we won't generate any meaningful interval with
          # given minimum.
          return None
        return counter


class DeoptFuzzer(Fuzzer):
  def __init__(self, min_interval):
    super(DeoptFuzzer, self).__init__()
    self._min = min_interval

  def create_flags_generator(self, rng, test, analysis_value):
    while True:
      if analysis_value:
        value = analysis_value / 2
      else:
        value = 10000
      interval = rng.randint(self._min, max(value, self._min))
      yield ['--deopt-every-n-times=%d' % interval]


FUZZERS = {
  'compaction': (None, CompactionFuzzer),
  'deopt': (DeoptAnalyzer, DeoptFuzzer),
  'gc_interval': (GcIntervalAnalyzer, GcIntervalFuzzer),
  'interrupt_budget': (None, InterruptBudgetFuzzer),
  'marking': (MarkingAnalyzer, MarkingFuzzer),
  'scavenge': (ScavengeAnalyzer, ScavengeFuzzer),
  'threads': (None, ThreadPoolSizeFuzzer),
}


def create_fuzzer_config(name, probability, *args, **kwargs):
  analyzer_class, fuzzer_class = FUZZERS[name]
  return FuzzerConfig(
      probability,
      analyzer_class(*args, **kwargs) if analyzer_class else None,
      fuzzer_class(*args, **kwargs),
  )
