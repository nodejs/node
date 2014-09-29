# Copyright 2011 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'All',
      'type': 'none',
      'dependencies': [
        '../samples/samples.gyp:*',
        '../src/d8.gyp:d8',
        '../test/base-unittests/base-unittests.gyp:*',
        '../test/cctest/cctest.gyp:*',
        '../test/compiler-unittests/compiler-unittests.gyp:*',
      ],
      'conditions': [
        ['component!="shared_library"', {
          'dependencies': [
            '../tools/lexer-shell.gyp:lexer-shell',
            '../tools/lexer-shell.gyp:parser-shell',
          ],
        }],
      ]
    }
  ]
}
