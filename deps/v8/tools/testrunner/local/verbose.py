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


import sys
import time

from . import statusfile


REPORT_TEMPLATE = (
"""Total: %(total)i tests
 * %(skipped)4d tests will be skipped
 * %(timeout)4d tests are expected to timeout sometimes
 * %(nocrash)4d tests are expected to be flaky but not crash
 * %(pass)4d tests are expected to pass
 * %(fail_ok)4d tests are expected to fail that we won't fix
 * %(fail)4d tests are expected to fail that we should fix""")


def PrintReport(tests):
  total = len(tests)
  skipped = timeout = nocrash = passes = fail_ok = fail = 0
  for t in tests:
    if "outcomes" not in dir(t) or not t.outcomes:
      passes += 1
      continue
    o = t.outcomes
    if statusfile.DoSkip(o):
      skipped += 1
      continue
    if statusfile.TIMEOUT in o: timeout += 1
    if statusfile.IsPassOrFail(o): nocrash += 1
    if list(o) == [statusfile.PASS]: passes += 1
    if statusfile.IsFailOk(o): fail_ok += 1
    if list(o) == [statusfile.FAIL]: fail += 1
  print REPORT_TEMPLATE % {
    "total": total,
    "skipped": skipped,
    "timeout": timeout,
    "nocrash": nocrash,
    "pass": passes,
    "fail_ok": fail_ok,
    "fail": fail
  }


def PrintTestSource(tests):
  for test in tests:
    suite = test.suite
    source = suite.GetSourceForTest(test).strip()
    if len(source) > 0:
      print "--- begin source: %s/%s ---" % (suite.name, test.path)
      print source
      print "--- end source: %s/%s ---" % (suite.name, test.path)


def FormatTime(d):
  millis = round(d * 1000) % 1000
  return time.strftime("%M:%S.", time.gmtime(d)) + ("%03i" % millis)


def PrintTestDurations(suites, overall_time):
    # Write the times to stderr to make it easy to separate from the
    # test output.
    print
    sys.stderr.write("--- Total time: %s ---\n" % FormatTime(overall_time))
    timed_tests = [ t for s in suites for t in s.tests
                    if t.duration is not None ]
    timed_tests.sort(lambda a, b: cmp(b.duration, a.duration))
    index = 1
    for entry in timed_tests[:20]:
      t = FormatTime(entry.duration)
      sys.stderr.write("%4i (%s) %s\n" % (index, t, entry.GetLabel()))
      index += 1
