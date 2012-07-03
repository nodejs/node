# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


"""Top-level presubmit script for GYP.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into gcl.
"""


PYLINT_BLACKLIST = [
    # TODO: fix me.
    # From SCons, not done in google style.
    'test/lib/TestCmd.py',
    'test/lib/TestCommon.py',
    'test/lib/TestGyp.py',
    # Needs style fix.
    'pylib/gyp/generator/scons.py',
    'pylib/gyp/generator/xcode.py',
]


PYLINT_DISABLED_WARNINGS = [
    # TODO: fix me.
    # Many tests include modules they don't use.
    'W0611',
    # Include order doesn't properly include local files?
    'F0401',
    # Some use of built-in names.
    'W0622',
    # Some unused variables.
    'W0612',
    # Operator not preceded/followed by space.
    'C0323',
    'C0322',
    # Unnecessary semicolon.
    'W0301',
    # Unused argument.
    'W0613',
    # String has no effect (docstring in wrong place).
    'W0105',
    # Comma not followed by space.
    'C0324',
    # Access to a protected member.
    'W0212',
    # Bad indent.
    'W0311',
    # Line too long.
    'C0301',
    # Undefined variable.
    'E0602',
    # Not exception type specified.
    'W0702',
    # No member of that name.
    'E1101',
    # Dangerous default {}.
    'W0102',
    # Others, too many to sort.
    'W0201', 'W0232', 'E1103', 'W0621', 'W0108', 'W0223', 'W0231',
    'R0201', 'E0101', 'C0321',
    # ************* Module copy
    # W0104:427,12:_test.odict.__setitem__: Statement seems to have no effect
    'W0104',
]


def CheckChangeOnUpload(input_api, output_api):
  report = []
  report.extend(input_api.canned_checks.PanProjectChecks(
      input_api, output_api))
  return report


def CheckChangeOnCommit(input_api, output_api):
  report = []
  license = (
      r'.*? Copyright \(c\) %(year)s Google Inc\. All rights reserved\.\n'
      r'.*? Use of this source code is governed by a BSD-style license that '
        r'can be\n'
      r'.*? found in the LICENSE file\.\n'
  ) % {
      'year': input_api.time.strftime('%Y'),
  }

  report.extend(input_api.canned_checks.PanProjectChecks(
      input_api, output_api, license_header=license))
  report.extend(input_api.canned_checks.CheckTreeIsOpen(
      input_api, output_api,
      'http://gyp-status.appspot.com/status',
      'http://gyp-status.appspot.com/current'))

  import sys
  old_sys_path = sys.path
  try:
    sys.path = ['pylib', 'test/lib'] + sys.path
    report.extend(input_api.canned_checks.RunPylint(
        input_api,
        output_api,
        black_list=PYLINT_BLACKLIST,
        disabled_warnings=PYLINT_DISABLED_WARNINGS))
  finally:
    sys.path = old_sys_path
  return report


def GetPreferredTrySlaves():
  return ['gyp-win32', 'gyp-win64', 'gyp-linux', 'gyp-mac']
