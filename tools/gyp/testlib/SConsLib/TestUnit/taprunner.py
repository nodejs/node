"""
Format unittest results in Test Anything Protocol (TAP).
http://testanything.org/tap-version-13-specification.html

Public domain work by:
  anatoly techtonik <techtonik@gmail.com>

Changes:
  0.3 - fixed used imports that failed on Python 2.6
  0.2 - removed unused import that failed on Python 2.6
  0.1 - initial release
"""

__version__ = "0.3"


from unittest import TextTestRunner
try:
    from unittest import TextTestResult
except ImportError:
    # Python 2.6
    from unittest import _TextTestResult as TextTestResult


class TAPTestResult(TextTestResult):

    def _process(self, test, msg, failtype = None, directive = None):
        """ increase the counter, format and output TAP info """
        # counterhack: increase test counter
        test.suite.tap_counter += 1
        msg = "%s %d" % (msg, test.suite.tap_counter)
        if "not" not in msg:
            msg += "    "  # justify
        self.stream.write("%s - " % msg)
        if failtype:
            self.stream.write("%s - " % failtype)
        self.stream.write("%s" % test.__class__.__name__)
        self.stream.write(".%s" % test._testMethodName)
        if directive:
            self.stream.write(directive)
        self.stream.write("\n")
        # [ ] write test __doc__ (if exists) in comment
        self.stream.flush()

    def addSuccess(self, test):
        super(TextTestResult, self).addSuccess(test)
        self._process(test, "ok")

    def addFailure(self, test, err):
        super(TextTestResult, self).addFailure(test, err)
        self._process(test, "not ok", "FAIL")
        # [ ] add structured data about assertion

    def addError(self, test, err):
        super(TextTestResult, self).addError(test, err)
        self._process(test, "not ok", "ERROR")
        # [ ] add structured data about exception

    def addSkip(self, test, reason):
        super(TextTestResult, self).addSkip(test, reason)
        self._process(test, "ok", directive=("  # SKIP  %s" % reason))

    def addExpectedFailure(self, test, err):
        super(TextTestResult, self).addExpectedFailure(test, err)
        self._process(test, "not ok", directive=("  # TODO"))

    def addUnexpectedSuccess(self, test):
        super(TextTestResult, self).addUnexpectedSuccess(test)
        self._process(test, "not ok", "FAIL (unexpected success)")

    """
    def printErrors(self):
    def printErrorList(self, flavour, errors):
    """


class TAPTestRunner(TextTestRunner):
    resultclass = TAPTestResult

    def run(self, test):
        self.stream.write("TAP version 13\n")
        # [ ] add commented block with test suite __doc__
        # [ ] check call with a single test
        # if isinstance(test, suite.TestSuite):
        self.stream.write("1..%s\n" % len(list(test)))

        # counterhack: inject test counter into test suite
        test.tap_counter = 0
        # counterhack: inject reference to suite into each test case
        for case in test:
            case.suite = test

        return super(TAPTestRunner, self).run(test)


if __name__ == "__main__":
    import sys
    import unittest

    class Test(unittest.TestCase):
       def test_ok(self):
           pass
       def test_fail(self):
           self.assertTrue(False)
       def test_error(self):
           bad_symbol
       @unittest.skip("skipin'")
       def test_skip(self):
           pass
       @unittest.expectedFailure
       def test_not_ready(self):
           self.fail()
       @unittest.expectedFailure
       def test_invalid_fail_mark(self):
           pass
       def test_another_ok(self):
           pass


    suite = unittest.TestSuite([
       Test('test_ok'),
       Test('test_fail'),
       Test('test_error'),
       Test('test_skip'),
       Test('test_not_ready'),
       Test('test_invalid_fail_mark'),
       Test('test_another_ok')
    ])
    if not TAPTestRunner().run(suite).wasSuccessful():
       sys.exit(1)
