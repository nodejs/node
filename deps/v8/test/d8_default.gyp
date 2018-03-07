# Copyright 2018 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'conditions': [
    ['test_isolation_mode != "noop"', {
      'targets': [
        {
          'target_name': 'd8_default_run',
          'type': 'none',
          'dependencies': [
            'debugger/debugger.gyp:debugger_run',
            'intl/intl.gyp:intl_run',
            'message/message.gyp:message_run',
            'mjsunit/mjsunit.gyp:mjsunit_run',
            'preparser/preparser.gyp:preparser_run',
            'webkit/webkit.gyp:webkit_run',
          ],
          'includes': [
            '../gypfiles/features.gypi',
            '../gypfiles/isolate.gypi',
          ],
          'sources': [
            'd8_default.isolate',
          ],
        },
      ],
    }],
  ],
}
