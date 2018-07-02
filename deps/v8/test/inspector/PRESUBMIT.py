#!/usr/bin/env python
#
# Copyright 2018 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into gcl.
"""


def PostUploadHook(cl, change, output_api):
  """git cl upload will call this hook after the issue is created/modified.

  This hook adds extra try bots to the CL description in order to run layout
  tests in addition to CQ try bots.
  """
  return output_api.EnsureCQIncludeTrybotsAreAdded(
    cl,
    [
      'master.tryserver.blink:linux_trusty_blink_rel',
      'luci.chromium.try:linux_chromium_headless_rel',
    ],
    'Automatically added layout test trybots to run tests on CQ.')
