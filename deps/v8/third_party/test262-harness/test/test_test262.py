#!/usr/bin/env python3

# Copyright 2014 by Sam Mikes.  All rights reserved.
# This code is governed by the BSD license found in the LICENSE file.

import unittest

import sys
import os
import cStringIO
from functools import wraps

sys.path.append("src")

import test262

class TestTest262(unittest.TestCase):

    def test_that_tests_run(self):
        self.assertEqual(1 + 2, 3)

class MockTest(object):

    def __init__(self, name, negative):
        self.name = name
        self.negative = negative if negative else False
        self.strict_mode = False

    def GetName(self):
        return self.name

    def IsNegative(self):
        return self.negative

    def GetMode(self):
        if self.strict_mode:
            return "strict mode"

        return "non-strict mode"

class MockResult(object):

    def __init__(self, case):
        self.case = case



class TestTestSuite(unittest.TestCase):

    def test_that_tests_run(self):
        self.assertEqual(1 + 2, 3)

    def test_create_test_suite(self):
        test_suite = test262.TestSuite(".",
                                       False,
                                       False,
                                       False,
                                       None)
        self.assertNotEqual(test_suite, None)

    def test_summary(self):
        test_suite = test262.TestSuite(".",
                                       False,
                                       False,
                                       False,
                                       None)

        progress = test262.ProgressIndicator(100)
        progress.succeeded = 98
        progress.failed = 2

        result = mute(True)(test_suite.PrintSummary)(progress, None)
        self.assertEqual("""
=== Summary ===
 - Ran 100 tests
 - Passed 98 tests (98.0%)
 - Failed 2 tests (2.0%)
""", result)

    def test_summary_logfile(self):
        test_suite = test262.TestSuite(".",
                                       False,
                                       False,
                                       False,
                                       None)

        progress = test262.ProgressIndicator(100)
        progress.succeeded = 98
        progress.failed = 2

        fake_log = cStringIO.StringIO()
        test_suite.logf = fake_log

        result = mute(True)(test_suite.PrintSummary)(progress, True)

        expected_out = """
=== Summary ===
 - Ran 100 tests
 - Passed 98 tests (98.0%)
 - Failed 2 tests (2.0%)
"""

        expected_log = """=== Summary ===
 - Ran 100 tests
 - Passed 98 tests (98.0%)
 - Failed 2 tests (2.0%)
"""
        self.assertEqual(expected_out, result)
        self.assertEqual(expected_log, fake_log.getvalue())


    def test_summary_withfails(self):
        test_suite = test262.TestSuite(".",
                                       False,
                                       False,
                                       False,
                                       None)

        progress = test262.ProgressIndicator(100)
        progress.succeeded = 98
        progress.failed = 2
        progress.failed_tests = [
            MockResult(MockTest("foo", False)),
            MockResult(MockTest("bar", True))
        ]

        result = mute(True)(test_suite.PrintSummary)(progress, None)
        self.assertEqual("""
=== Summary ===
 - Ran 100 tests
 - Passed 98 tests (98.0%)
 - Failed 2 tests (2.0%)

Failed Tests
  foo in non-strict mode

Expected to fail but passed ---
  bar in non-strict mode
""", result)


    def test_summary_withfails_andlog(self):
        test_suite = test262.TestSuite(".",
                                       False,
                                       False,
                                       False,
                                       None)

        progress = test262.ProgressIndicator(100)
        progress.succeeded = 98
        progress.failed = 2
        progress.failed_tests = [
            MockResult(MockTest("foo", False)),
            MockResult(MockTest("bar", True))
        ]

        fake_log = cStringIO.StringIO()
        test_suite.logf = fake_log

        expected_out = """
=== Summary ===
 - Ran 100 tests
 - Passed 98 tests (98.0%)
 - Failed 2 tests (2.0%)

Failed Tests
  foo in non-strict mode

Expected to fail but passed ---
  bar in non-strict mode
"""
        expected_log = """=== Summary ===
 - Ran 100 tests
 - Passed 98 tests (98.0%)
 - Failed 2 tests (2.0%)
Failed Tests
  foo in non-strict mode
Expected to fail but passed ---
  bar in non-strict mode
"""

        result = mute(True)(test_suite.PrintSummary)(progress, True)
        self.assertEqual(expected_out, result)
        self.assertEqual(expected_log, fake_log.getvalue())


    def test_summary_success_logfile(self):
        test_suite = test262.TestSuite(".",
                                       False,
                                       False,
                                       False,
                                       None)

        progress = test262.ProgressIndicator(100)
        progress.succeeded = 100
        progress.failed = 0

        fake_log = cStringIO.StringIO()
        test_suite.logf = fake_log

        result = mute(True)(test_suite.PrintSummary)(progress, True)

        expected_out = """
=== Summary ===
 - Ran 100 tests
 - All tests succeeded
"""

        expected_log = """=== Summary ===
 - Ran 100 tests
 - All tests succeeded
"""
        self.assertEqual(expected_out, result)
        self.assertEqual(expected_log, fake_log.getvalue())


    def test_percent_format(self):
        self.assertEqual(test262.PercentFormat(1, 100), "1 test (1.0%)")
        self.assertEqual(test262.PercentFormat(0, 100), "0 tests (0.0%)")
        self.assertEqual(test262.PercentFormat(99, 100), "99 tests (99.0%)")


# module level utility functions
# copied from https://stackoverflow.com/questions/2828953/silence-the-stdout-of-a-function-in-python-without-trashing-sys-stdout-and-resto


def mute(returns_output=False):
    """
        Decorate a function that prints to stdout, intercepting the output.
        If "returns_output" is True, the function will return a generator
        yielding the printed lines instead of the return values.

        The decorator litterally hijack sys.stdout during each function
        execution for ALL THE THREADS, so be careful with what you apply it to
        and in which context.

        >>> def numbers():
            print "42"
            print "1984"
        ...
        >>> numbers()
        42
        1984
        >>> mute()(numbers)()
        >>> list(mute(True)(numbers)())
        ['42', '1984']

    """

    def decorator(func):

        @wraps(func)
        def wrapper(*args, **kwargs):

            saved_stdout = sys.stdout
            sys.stdout = cStringIO.StringIO()

            try:
                out = func(*args, **kwargs)
                if returns_output:
                    out = sys.stdout.getvalue()
            finally:
                sys.stdout = saved_stdout

            return out

        return wrapper

    return decorator


if __name__ == '__main__':
    unittest.main()

