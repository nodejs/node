#!/usr/bin/env python
# Copyright 2016 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import statusfile
from utils import Freeze


TEST_VARIABLES = {
  'system': 'linux',
  'mode': 'release',
}


TEST_STATUS_FILE = """
[
[ALWAYS, {
  'foo/bar': [PASS, SKIP],
  'baz/bar': [PASS, FAIL],
  'foo/*': [PASS, SLOW],
}],  # ALWAYS

['%s', {
  'baz/bar': [PASS, SLOW],
  'foo/*': [FAIL],
}],
]
"""


def make_variables():
  variables = {}
  variables.update(TEST_VARIABLES)
  return variables


class UtilsTest(unittest.TestCase):
  def test_freeze(self):
    self.assertEqual(2, Freeze({1: [2]})[1][0])
    self.assertEqual(set([3]), Freeze({1: [2], 2: set([3])})[2])

    with self.assertRaises(Exception):
      Freeze({1: [], 2: set([3])})[2] = 4
    with self.assertRaises(Exception):
      Freeze({1: [], 2: set([3])}).update({3: 4})
    with self.assertRaises(Exception):
      Freeze({1: [], 2: set([3])})[1].append(2)
    with self.assertRaises(Exception):
      Freeze({1: [], 2: set([3])})[2] |= set([3])

    # Sanity check that we can do the same calls on a non-frozen object.
    {1: [], 2: set([3])}[2] = 4
    {1: [], 2: set([3])}.update({3: 4})
    {1: [], 2: set([3])}[1].append(2)
    {1: [], 2: set([3])}[2] |= set([3])


class StatusFileTest(unittest.TestCase):
  def test_eval_expression(self):
    variables = make_variables()
    variables.update(statusfile.VARIABLES)

    self.assertTrue(
        statusfile._EvalExpression(
            'system==linux and mode==release', variables))
    self.assertTrue(
        statusfile._EvalExpression(
            'system==linux or variant==default', variables))
    self.assertFalse(
        statusfile._EvalExpression(
            'system==linux and mode==debug', variables))
    self.assertRaises(
        AssertionError,
        lambda: statusfile._EvalExpression(
            'system==linux and mode==foo', variables))
    self.assertRaises(
        SyntaxError,
        lambda: statusfile._EvalExpression(
            'system==linux and mode=release', variables))
    self.assertEquals(
        statusfile.VARIANT_EXPRESSION,
        statusfile._EvalExpression(
            'system==linux and variant==default', variables)
    )

  def test_read_statusfile_section_true(self):
    rules, wildcards = statusfile.ReadStatusFile(
        TEST_STATUS_FILE % 'system==linux', make_variables())

    self.assertEquals(
        {
          'foo/bar': set(['PASS', 'SKIP']),
          'baz/bar': set(['PASS', 'FAIL', 'SLOW']),
        },
        rules[''],
    )
    self.assertEquals(
        {
          'foo/*': set(['SLOW', 'FAIL']),
        },
        wildcards[''],
    )
    self.assertEquals({}, rules['default'])
    self.assertEquals({}, wildcards['default'])

  def test_read_statusfile_section_false(self):
    rules, wildcards = statusfile.ReadStatusFile(
        TEST_STATUS_FILE % 'system==windows', make_variables())

    self.assertEquals(
        {
          'foo/bar': set(['PASS', 'SKIP']),
          'baz/bar': set(['PASS', 'FAIL']),
        },
        rules[''],
    )
    self.assertEquals(
        {
          'foo/*': set(['PASS', 'SLOW']),
        },
        wildcards[''],
    )
    self.assertEquals({}, rules['default'])
    self.assertEquals({}, wildcards['default'])

  def test_read_statusfile_section_variant(self):
    rules, wildcards = statusfile.ReadStatusFile(
        TEST_STATUS_FILE % 'system==linux and variant==default',
        make_variables(),
    )

    self.assertEquals(
        {
          'foo/bar': set(['PASS', 'SKIP']),
          'baz/bar': set(['PASS', 'FAIL']),
        },
        rules[''],
    )
    self.assertEquals(
        {
          'foo/*': set(['PASS', 'SLOW']),
        },
        wildcards[''],
    )
    self.assertEquals(
        {
          'baz/bar': set(['PASS', 'SLOW']),
        },
        rules['default'],
    )
    self.assertEquals(
        {
          'foo/*': set(['FAIL']),
        },
        wildcards['default'],
    )


if __name__ == '__main__':
    unittest.main()
