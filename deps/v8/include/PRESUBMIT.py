# Copyright 2017 the V8 project authors. All rights reserved.')
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for //v8/include

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

import os


def PostUploadHook(cl, change, output_api):
  """git cl upload will call this hook after the issue is created/modified.

  This hook adds extra try bots to the CL description in order to run layout
  tests in addition to CQ try bots.
  """
  def header_filter(f):
    return '.h' in os.path.split(f.LocalPath())[1]
  if not change.AffectedFiles(file_filter=header_filter):
    return []
  return output_api.EnsureCQIncludeTrybotsAreAdded(
    cl,
    [
      'master.tryserver.chromium.linux:linux_chromium_rel_ng'
    ],
    'Automatically added layout test trybots to run tests on CQ.')
