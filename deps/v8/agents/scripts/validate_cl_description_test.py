#!/usr/bin/env python3
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import io
import unittest
from unittest import mock

from validate_cl_description import validate_description


class ValidateClDescriptionTest(unittest.TestCase):

  def setUp(self):
    super().setUp()
    self.run_patcher = mock.patch('validate_cl_description.subprocess.run')
    self.mock_run = self.run_patcher.start()
    self.mock_run.return_value = mock.Mock(stdout='', returncode=0)

  def tearDown(self):
    self.run_patcher.stop()
    super().tearDown()

  def test_short_title_and_valid_body_passes(self):
    desc = ('[agents] Enforce commit title length limit\n'
            '\n'
            'Ensure commit titles stay at most 64 characters and body lines\n'
            'stay within 72 characters.\n')
    self.assertTrue(validate_description(desc))

  def test_title_at_64_chars_passes(self):
    title = '[wasm] ' + 'a' * 57
    self.assertEqual(len(title), 64)
    self.assertTrue(validate_description(f'{title}\n\nBody text.\n'))

  def test_title_over_64_chars_fails(self):
    title = '[wasm] ' + 'a' * 58
    self.assertEqual(len(title), 65)
    stderr = io.StringIO()
    with mock.patch('sys.stderr', stderr):
      self.assertFalse(validate_description(f'{title}\n\nBody text.\n'))
    self.assertIn('65 characters long', stderr.getvalue())
    self.assertIn('exceeding the 64-character maximum length limit',
                  stderr.getvalue())

  def test_body_line_at_72_chars_passes(self):
    line_72 = ('word ' * 14) + 'ab'
    self.assertEqual(len(line_72), 72)
    desc = f'[wasm] Short title\n\n{line_72}\n'
    self.assertTrue(validate_description(desc))

  def test_body_line_over_72_chars_fails(self):
    long_line = ('word ' * 14) + 'abc'  # 73 chars
    self.assertEqual(len(long_line), 73)
    desc = f'[wasm] Short title\n\n{long_line}\n'
    stderr = io.StringIO()
    with mock.patch('sys.stderr', stderr):
      self.assertFalse(validate_description(desc))
    self.assertIn('exceeding the 72 character', stderr.getvalue())

  def test_long_url_in_body_passes(self):
    url_line = 'See https://chromium-review.googlesource.com/c/v8/v8/+/8517730/1/agents/rules/git-commit.md'
    self.assertGreater(len(url_line), 72)
    desc = f'[wasm] Short title\n\n{url_line}\n'
    self.assertTrue(validate_description(desc))


if __name__ == '__main__':
  unittest.main()
