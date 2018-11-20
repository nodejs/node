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
 * %(nocrash)4d tests are expected to be flaky but not crash
 * %(pass)4d tests are expected to pass
 * %(fail_ok)4d tests are expected to fail that we won't fix
 * %(fail)4d tests are expected to fail that we should fix
 * %(crash)4d tests are expected to crash
""")


# TODO(majeski): Turn it into an observer.
def PrintReport(tests):
  total = len(tests)
  skipped = nocrash = passes = fail_ok = fail = crash = 0
  for t in tests:
    if t.do_skip:
      skipped += 1
    elif t.is_pass_or_fail:
      nocrash += 1
    elif t.is_fail_ok:
      fail_ok += 1
    elif t.expected_outcomes == [statusfile.PASS]:
      passes += 1
    elif t.expected_outcomes == [statusfile.FAIL]:
      fail += 1
    elif t.expected_outcomes == [statusfile.CRASH]:
      crash += 1
    else:
      assert False # Unreachable # TODO: check this in outcomes parsing phase.

  print REPORT_TEMPLATE % {
    "total": total,
    "skipped": skipped,
    "nocrash": nocrash,
    "pass": passes,
    "fail_ok": fail_ok,
    "fail": fail,
    "crash": crash,
  }


def PrintTestSource(tests):
  for test in tests:
    print "--- begin source: %s ---" % test
    if test.is_source_available():
      print test.get_source()
    else:
      print '(no source available)'
    print "--- end source: %s ---" % test


def FormatTime(d):
  millis = round(d * 1000) % 1000
  return time.strftime("%M:%S.", time.gmtime(d)) + ("%03i" % millis)


def PrintTestDurations(suites, outputs, overall_time):
    # Write the times to stderr to make it easy to separate from the
    # test output.
    print
    sys.stderr.write("--- Total time: %s ---\n" % FormatTime(overall_time))
    timed_tests = [(t, outputs[t].duration) for s in suites for t in s.tests
                   if t in outputs]
    timed_tests.sort(key=lambda (_, duration): duration, reverse=True)
    index = 1
    for test, duration in timed_tests[:20]:
      t = FormatTime(duration)
      sys.stderr.write("%4i (%s) %s\n" % (index, t, test))
      index += 1
