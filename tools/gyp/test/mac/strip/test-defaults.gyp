# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'make_global_settings': [
    ['CC', '/usr/bin/clang'],
  ],
  'target_defaults': {
    'xcode_settings': {
      'GCC_VERSION': 'com.apple.compilers.llvm.clang.1_0',
      'DEPLOYMENT_POSTPROCESSING': 'YES',
      'STRIP_INSTALLED_PRODUCT': 'YES',
    },
  },
  'targets': [
    {
      'target_name': 'single_dylib',
      'type': 'shared_library',
      'sources': [ 'file.c', ],
    },
    {
      'target_name': 'single_so',
      'type': 'loadable_module',
      'sources': [ 'file.c', ],
    },
    {
      'target_name': 'single_exe',
      'type': 'executable',
      'sources': [ 'main.c', ],
    },

    {
      'target_name': 'bundle_dylib',
      'type': 'shared_library',
      'mac_bundle': '1',
      'sources': [ 'file.c', ],
    },
    {
      'target_name': 'bundle_so',
      'type': 'loadable_module',
      'mac_bundle': '1',
      'sources': [ 'file.c', ],
    },
    {
      'target_name': 'bundle_exe',
      'type': 'executable',
      'mac_bundle': '1',
      'sources': [ 'main.c', ],
    },
  ],
}
