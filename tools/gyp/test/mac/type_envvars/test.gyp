# Copyright (c) 2011 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'my_app',
      'product_name': 'My App',
      'type': 'executable',
      'mac_bundle': 1,
      'sources': [ 'file.c', ],
      'postbuilds': [
        {
          'postbuild_name': 'envtest',
          'action': [ './test_bundle_executable.sh', ],
        },
      ],
    },
    {
      'target_name': 'bundle_loadable_module',
      'type': 'loadable_module',
      'mac_bundle': 1,
      'sources': [ 'file.c', ],
      'postbuilds': [
        {
          'postbuild_name': 'envtest',
          'action': [ './test_bundle_loadable_module.sh', ],
        },
      ],
    },
    {
      'target_name': 'bundle_shared_library',
      'type': 'shared_library',
      'mac_bundle': 1,
      'sources': [ 'file.c', ],
      'postbuilds': [
        {
          'postbuild_name': 'envtest',
          'action': [ './test_bundle_shared_library.sh', ],
        },
      ],
    },
    # Types 'static_library' and 'none' can't exist as bundles.

    {
      'target_name': 'nonbundle_executable',
      'type': 'executable',
      'sources': [ 'file.c', ],
      'postbuilds': [
        {
          'postbuild_name': 'envtest',
          'action': [ './test_nonbundle_executable.sh', ],
        },
      ],
    },
    {
      'target_name': 'nonbundle_loadable_module',
      'type': 'loadable_module',
      'sources': [ 'file.c', ],
      'postbuilds': [
        {
          'postbuild_name': 'envtest',
          'action': [ './test_nonbundle_loadable_module.sh', ],
        },
      ],
    },
    {
      'target_name': 'nonbundle_shared_library',
      'type': 'shared_library',
      'sources': [ 'file.c', ],
      'postbuilds': [
        {
          'postbuild_name': 'envtest',
          'action': [ './test_nonbundle_shared_library.sh', ],
        },
      ],
    },
    {
      'target_name': 'nonbundle_static_library',
      'type': 'static_library',
      'sources': [ 'file.c', ],
      'postbuilds': [
        {
          'postbuild_name': 'envtest',
          'action': [ './test_nonbundle_static_library.sh', ],
        },
      ],
    },
    {
      'target_name': 'nonbundle_none',
      'type': 'none',
      'postbuilds': [
        {
          'postbuild_name': 'envtest',
          'action': [ './test_nonbundle_none.sh', ],
        },
      ],
    },
  ],
}
