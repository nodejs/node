# Copyright 2016 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'conditions': [
    ['test_isolation_mode != "noop"', {
      'targets': [
        {
          'target_name': 'perf_run',
          'type': 'none',
          'dependencies': [
            'cctest/cctest.gyp:cctest_exe_run',
            '../src/d8.gyp:d8_run',
          ],
          'includes': [
            '../gypfiles/features.gypi',
            '../gypfiles/isolate.gypi',
          ],
          'sources': [
            'perf.isolate',
          ],
        },
      ],
    }],
  ],
}
