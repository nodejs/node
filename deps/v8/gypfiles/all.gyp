# Copyright 2011 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'All',
      'type': 'none',
      'dependencies': [
        '../src/d8.gyp:d8',
      ],
      'conditions': [
        ['component!="shared_library"', {
          'dependencies': [
            '../tools/parser-shell.gyp:parser-shell',
          ],
        }],
        # These items don't compile for Android on Mac.
        ['host_os!="mac" or OS!="android"', {
          'dependencies': [
            '../samples/samples.gyp:*',
            '../test/cctest/cctest.gyp:*',
            '../test/fuzzer/fuzzer.gyp:*',
            '../test/unittests/unittests.gyp:*',
          ],
        }],
        ['v8_enable_inspector==1', {
          'dependencies': [
            '../test/inspector/inspector.gyp:*',
          ],
        }],
        ['v8_enable_inspector==1 and test_isolation_mode != "noop"', {
          'dependencies': [
            '../test/debugger/debugger.gyp:*',
          ],
        }],
        ['test_isolation_mode != "noop"', {
          'dependencies': [
            '../test/bot_default.gyp:*',
            '../test/benchmarks/benchmarks.gyp:*',
            '../test/default.gyp:*',
            '../test/intl/intl.gyp:*',
            '../test/message/message.gyp:*',
            '../test/mjsunit/mjsunit.gyp:*',
            '../test/mozilla/mozilla.gyp:*',
            '../test/optimize_for_size.gyp:*',
            '../test/perf.gyp:*',
            '../test/preparser/preparser.gyp:*',
            '../test/simdjs/simdjs.gyp:*',
            '../test/test262/test262.gyp:*',
            '../test/webkit/webkit.gyp:*',
            '../tools/check-static-initializers.gyp:*',
            '../tools/gcmole/run_gcmole.gyp:*',
            '../tools/jsfunfuzz/jsfunfuzz.gyp:*',
            '../tools/run-deopt-fuzzer.gyp:*',
            '../tools/run-valgrind.gyp:*',
          ],
        }],
      ]
    }
  ]
}
