# Copyright 2022 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import logging
import pprint
import os

from . import base
from .indicators import (
    formatted_result_output,
    ProgressIndicator,
)
from .util import (
    base_test_record,
    extract_tags,
    strip_ascii_control_characters,
)


class ResultDBIndicator(ProgressIndicator):

  def __init__(self, context, options, test_count, sink):
    super(ResultDBIndicator, self).__init__(context, options, test_count)
    self._requirement = base.DROP_PASS_OUTPUT
    self.rpc = ResultDB_RPC(sink)

  def on_test_result(self, test, result):
    for run, sub_result in enumerate(result.as_list):
      self.send_result(test, sub_result, run)

  def send_result(self, test, result, run):
    # We need to recalculate the observed (but lost) test behaviour.
    # `result.has_unexpected_output` indicates that the run behaviour of the
    # test matches the expected behaviour irrespective of passing or failing.
    result_expected = not result.has_unexpected_output
    test_should_pass = not test.is_fail
    run_passed = (result_expected == test_should_pass)
    rdb_result = {
        'testId': strip_ascii_control_characters(test.full_name),
        'status': 'PASS' if run_passed else 'FAIL',
        'expected': result_expected,
    }

    if result.output and result.output.duration:
      rdb_result.update(duration=f'{result.output.duration}ms')
    if result.has_unexpected_output:
      formated_output = formatted_result_output(result)
      sanitized = strip_ascii_control_characters(formated_output)
      # TODO(liviurau): do we have a better presentation data for this?
      # Protobuf strings can have len == 2**32.
      rdb_result.update(summaryHtml=f'<pre>{sanitized}</pre>')
    record = base_test_record(test, result, run)
    rdb_result.update(tags=extract_tags(record))
    self.rpc.send(rdb_result)


def rdb_sink():
  try:
    import requests
  except:
    log_instantiation_failure('Failed to import requests module.')
    return None
  luci_context = os.environ.get('LUCI_CONTEXT')
  if not luci_context:
    log_instantiation_failure('No LUCI_CONTEXT found.')
    return None
  with open(luci_context, mode="r", encoding="utf-8") as f:
    config = json.load(f)
  sink = config.get('result_sink', None)
  if not sink:
    log_instantiation_failure('No ResultDB sink found.')
    return None
  return sink


def log_instantiation_failure(error_message):
  logging.info(f'{error_message} No results will be sent to ResultDB.')


class ResultDB_RPC:

  def __init__(self, sink):
    import requests
    self.session = requests.Session()
    self.session.headers = {
        'Authorization': f'ResultSink {sink.get("auth_token")}',
    }
    self.url = f'http://{sink.get("address")}/prpc/luci.resultsink.v1.Sink/ReportTestResults'

  def send(self, result):
    payload = dict(testResults=[result])
    try:
      self.session.post(self.url, json=payload).raise_for_status()
    except Exception as e:
      logging.error(f'Request failed: {payload}')
      raise e

  def __del__(self):
    self.session.close()
