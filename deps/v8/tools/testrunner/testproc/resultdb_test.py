# Copyright 2023 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from collections import namedtuple
from http.server import BaseHTTPRequestHandler, HTTPServer
from threading import Thread
from typing import Any

import json
import os
import requests
import socket
import sys
import unittest

# Needed because the test runner contains relative imports.
TOOLS_PATH = os.path.dirname(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.append(TOOLS_PATH)

# Standard library imports...
from testrunner.testproc.resultdb import ResultDBIndicator
from testrunner.local.command import BaseCommand
from testrunner.objects.output import Output


class RDBRequestHandler(BaseHTTPRequestHandler):

  def do_POST(self):
    content_length = int(self.headers['Content-Length'])
    post_data = self.rfile.read(content_length)
    self.server.register_post_data(post_data)
    self.send_response(requests.codes.ok)
    self.end_headers()
    return


def get_free_port():
  s = socket.socket(socket.AF_INET, type=socket.SOCK_STREAM)
  s.bind(('localhost', 0))
  _, port = s.getsockname()
  s.close()
  return port


class RDBMockServer(HTTPServer):

  def __init__(self) -> None:
    self.port = get_free_port()
    super().__init__(('localhost', self.port), RDBRequestHandler)
    self.thread = Thread(target=self.serve_forever)
    self.thread.daemon = True
    self.thread.start()
    self.post_data = []

  def register_post_data(self, data):
    self.post_data.append(data)

  @property
  def address(self):
    return f'localhost:{self.port}'


def mock_test262(is_fail, rdb_test_id):
  Test = namedtuple('Test', [
      'is_fail', 'rdb_test_id', 'expected_outcomes', 'framework_name',
      'full_name', 'random_seed', 'shard_id', 'shard_count', 'shell', 'variant',
      'variant_flags', 'processor_name', 'subtest_id', 'path', 'skip_rdb'
  ])

  def skip_if_expected(result):
    return not result.has_unexpected_output

  return Test(is_fail, rdb_test_id, [], 'unittest', 'test-123', 0, 1, 2, '',
              'default', [], 'none', None, '/tmp/test/some_test.js',
              skip_if_expected)


def mock_result(is_fail, duration=None):
  Result = namedtuple('Result', ['has_unexpected_output', 'output', 'cmd'])
  return Result(is_fail, Output(duration=duration), BaseCommand('echo'))


class TestResultDBIndicator(unittest.TestCase):

  def test_send(self):

    def assert_sent_count(count):
      self.assertEqual(len(server.post_data), count)

    server = RDBMockServer()
    self.rdb = ResultDBIndicator(
        context=None,
        options=None,
        test_count=0,
        sink=dict(auth_token='none', address=server.address))

    self.rdb.send_result(
        mock_test262(True, 'expected2fail-fail'), mock_result(False), 0)
    assert_sent_count(0)

    self.rdb.send_result(
        mock_test262(True, 'expected2fail-pass'), mock_result(True), 0)
    assert_sent_count(1)

    self.rdb.send_result(
        mock_test262(False, 'expected2pass-fail'), mock_result(True), 0)
    assert_sent_count(2)

    self.rdb.send_result(
        mock_test262(False, 'expected2pass-pass'), mock_result(False), 0)
    assert_sent_count(2)

    self.rdb.send_result(
        mock_test262(True, 'fast'), mock_result(True, duration=0.00001), 0)
    assert_sent_count(3)

    def assert_testid_at_index(testid, idx):
      data = json.loads(server.post_data[idx])
      self.assertEqual(data['testResults'][0]['testId'], testid)

    def assert_duration_at_index(duration_str, idx):
      data = json.loads(server.post_data[idx])
      self.assertEqual(data['testResults'][0]['duration'], duration_str)

    assert_testid_at_index('expected2fail-pass', 0)
    assert_testid_at_index('expected2pass-fail', 1)
    assert_duration_at_index('0.000010s', 2)


if __name__ == '__main__':
  unittest.main()
