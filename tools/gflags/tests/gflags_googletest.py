#!/usr/bin/env python

# Copyright (c) 2011, Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
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

"""Some simple additions to the unittest framework useful for gflags testing."""



import re
import unittest


def Sorted(lst):
  """Equivalent of sorted(), but not dependent on python version."""
  sorted_list = lst[:]
  sorted_list.sort()
  return sorted_list


def MultiLineEqual(expected, actual):
  """Returns True if expected == actual, or returns False and logs."""
  if actual == expected:
    return True

  print "Error: FLAGS.MainModuleHelp() didn't return the expected result."
  print "Got:"
  print actual
  print "[End of got]"

  actual_lines = actual.split("\n")
  expected_lines = expected.split("\n")

  num_actual_lines = len(actual_lines)
  num_expected_lines = len(expected_lines)

  if num_actual_lines != num_expected_lines:
    print "Number of actual lines = %d, expected %d" % (
        num_actual_lines, num_expected_lines)

  num_to_match = min(num_actual_lines, num_expected_lines)

  for i in range(num_to_match):
    if actual_lines[i] != expected_lines[i]:
      print "One discrepancy: Got:"
      print actual_lines[i]
      print "Expected:"
      print expected_lines[i]
      break
  else:
    # If we got here, found no discrepancy, print first new line.
    if num_actual_lines > num_expected_lines:
      print "New help line:"
      print actual_lines[num_expected_lines]
    elif num_expected_lines > num_actual_lines:
      print "Missing expected help line:"
      print expected_lines[num_actual_lines]
    else:
      print "Bug in this test -- discrepancy detected but not found."

  return False


class TestCase(unittest.TestCase):
  def assertListEqual(self, list1, list2):
    """Asserts that, when sorted, list1 and list2 are identical."""
    # This exists in python 2.7, but not previous versions.  Use the
    # built-in version if possible.
    if hasattr(unittest.TestCase, "assertListEqual"):
      unittest.TestCase.assertListEqual(self, Sorted(list1), Sorted(list2))
    else:
      self.assertEqual(Sorted(list1), Sorted(list2))

  def assertMultiLineEqual(self, expected, actual):
    # This exists in python 2.7, but not previous versions.  Use the
    # built-in version if possible.
    if hasattr(unittest.TestCase, "assertMultiLineEqual"):
      unittest.TestCase.assertMultiLineEqual(self, expected, actual)
    else:
      self.assertTrue(MultiLineEqual(expected, actual))

  def assertRaisesWithRegexpMatch(self, exception, regexp, fn, *args, **kwargs):
    try:
      fn(*args, **kwargs)
    except exception, why:
      self.assertTrue(re.search(regexp, str(why)),
                      "'%s' does not match '%s'" % (regexp, why))
      return
    self.fail(exception.__name__ + " not raised")


def main():
  unittest.main()
