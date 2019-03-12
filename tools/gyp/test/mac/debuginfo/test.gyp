# Copyright (c) 2011 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'nonbundle_static_library',
      'type': 'static_library',
      'sources': [ 'file.c', ],
      'xcode_settings': {
        'DEBUG_INFORMATION_FORMAT': 'dwarf-with-dsym',
        'DEPLOYMENT_POSTPROCESSING': 'YES',
        'STRIP_INSTALLED_PRODUCT': 'YES',
      },
    },
    {
      'target_name': 'nonbundle_shared_library',
      'type': 'shared_library',
      'sources': [ 'file.c', ],
      'xcode_settings': {
        'DEBUG_INFORMATION_FORMAT': 'dwarf-with-dsym',
        'DEPLOYMENT_POSTPROCESSING': 'YES',
        'STRIP_INSTALLED_PRODUCT': 'YES',
      },
    },
    {
      'target_name': 'nonbundle_loadable_module',
      'type': 'loadable_module',
      'sources': [ 'file.c', ],
      'xcode_settings': {
        'DEBUG_INFORMATION_FORMAT': 'dwarf-with-dsym',
        'DEPLOYMENT_POSTPROCESSING': 'YES',
        'STRIP_INSTALLED_PRODUCT': 'YES',
      },
    },
    {
      'target_name': 'nonbundle_executable',
      'type': 'executable',
      'sources': [ 'file.c', ],
      'xcode_settings': {
        'DEBUG_INFORMATION_FORMAT': 'dwarf-with-dsym',
        'DEPLOYMENT_POSTPROCESSING': 'YES',
        'STRIP_INSTALLED_PRODUCT': 'YES',
      },
    },

    {
      'target_name': 'bundle_shared_library',
      'type': 'shared_library',
      'mac_bundle': 1,
      'sources': [ 'file.c', ],
      'xcode_settings': {
        'DEBUG_INFORMATION_FORMAT': 'dwarf-with-dsym',
        'DEPLOYMENT_POSTPROCESSING': 'YES',
        'STRIP_INSTALLED_PRODUCT': 'YES',
      },
    },
    {
      'target_name': 'bundle_loadable_module',
      'type': 'loadable_module',
      'mac_bundle': 1,
      'sources': [ 'file.c', ],
      'xcode_settings': {
        'DEBUG_INFORMATION_FORMAT': 'dwarf-with-dsym',
        'DEPLOYMENT_POSTPROCESSING': 'YES',
        'STRIP_INSTALLED_PRODUCT': 'YES',
      },
    },
    {
      'target_name': 'my_app',
      'product_name': 'My App',
      'type': 'executable',
      'mac_bundle': 1,
      'sources': [ 'file.c', ],
      'xcode_settings': {
        'DEBUG_INFORMATION_FORMAT': 'dwarf-with-dsym',
        'DEPLOYMENT_POSTPROCESSING': 'YES',
        'STRIP_INSTALLED_PRODUCT': 'YES',
      },
    },
  ],
}
