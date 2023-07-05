# Copyright 2018 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


class ResultBase(object):
  @property
  def is_skipped(self):
    return False

  @property
  def is_grouped(self):
    return False

  @property
  def is_rerun(self):
    return False

  @property
  def as_list(self):
    return [self]


class Result(ResultBase):
  """Result created by the output processor."""

  def __init__(self,
               has_unexpected_output,
               output,
               cmd=None,
               error_details=None):
    self.has_unexpected_output = has_unexpected_output
    self.output = output
    self.cmd = cmd
    self.error_details = error_details

  def status(self):
    if self.has_unexpected_output:
      if not hasattr(self.output, "HasCrashed"):
        raise Exception(type(self))
      if self.output.HasCrashed():
        return 'CRASH'
      else:
        return 'FAIL'
    else:
      return 'PASS'


class GroupedResult(ResultBase):
  """Result consisting of multiple results. It can be used by processors that
  create multiple subtests for each test and want to pass all results back.
  """

  @staticmethod
  def create(results):
    """Create grouped result from the list of results. It filters out skipped
    results. If all results are skipped results it returns skipped result.

    Args:
      results: list of pairs (test, result)
    """
    results = [(t, r) for (t, r) in results if not r.is_skipped]
    if not results:
      return SKIPPED
    return GroupedResult(results)

  def __init__(self, results):
    self.results = results

  @property
  def is_grouped(self):
    return True


class SkippedResult(ResultBase):
  """Result without any meaningful value. Used primarily to inform the test
  processor that it's test wasn't executed.
  """

  @property
  def is_skipped(self):
    return True


SKIPPED = SkippedResult()


class RerunResult(Result):
  """Result generated from several reruns of the same test. It's a subclass of
  Result since the result of rerun is result of the last run. In addition to
  normal result it contains results of all reruns.
  """
  @staticmethod
  def create(results):
    """Create RerunResult based on list of results. List cannot be empty. If it
    has only one element it's returned as a result.
    """
    assert results

    if len(results) == 1:
      return results[0]
    return RerunResult(results)

  def __init__(self, results):
    """Has unexpected output and the output itself of the RerunResult equals to
    the last result in the passed list.
    """
    assert results

    last = results[-1]
    super(RerunResult, self).__init__(last.has_unexpected_output, last.output,
                                      last.cmd)
    self.results = results

  @property
  def is_rerun(self):
    return True

  @property
  def as_list(self):
    return self.results

  def status(self):
    return ' '.join(r.status() for r in self.results)
