# Copyright 2014 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'v8_code': 1,
  },
  'includes': ['../../build/toolchain.gypi', '../../build/features.gypi'],
  'targets': [
    {
      'target_name': 'compiler-unittests',
      'type': 'executable',
      'dependencies': [
        '../test/test.gyp:run-all-unittests',
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [  ### gcmole(all) ###
        'change-lowering-unittest.cc',
        'common-operator-unittest.cc',
        'compiler-test-utils.h',
        'graph-reducer-unittest.cc',
        'graph-unittest.cc',
        'graph-unittest.h',
        'instruction-selector-unittest.cc',
        'instruction-selector-unittest.h',
        'js-builtin-reducer-unittest.cc',
        'machine-operator-reducer-unittest.cc',
        'machine-operator-unittest.cc',
        'simplified-operator-reducer-unittest.cc',
        'simplified-operator-unittest.cc',
        'value-numbering-reducer-unittest.cc',
      ],
      'conditions': [
        ['v8_target_arch=="arm"', {
          'sources': [  ### gcmole(arch:arm) ###
            'arm/instruction-selector-arm-unittest.cc',
          ],
        }],
        ['v8_target_arch=="arm64"', {
          'sources': [  ### gcmole(arch:arm64) ###
            'arm64/instruction-selector-arm64-unittest.cc',
          ],
        }],
        ['v8_target_arch=="ia32"', {
          'sources': [  ### gcmole(arch:ia32) ###
            'ia32/instruction-selector-ia32-unittest.cc',
          ],
        }],
        ['v8_target_arch=="x64"', {
          'sources': [  ### gcmole(arch:x64) ###
            'x64/instruction-selector-x64-unittest.cc',
          ],
        }],
      ],
    },
  ],
}
