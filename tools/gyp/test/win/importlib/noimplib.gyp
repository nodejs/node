# Copyright (c) 2015 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
 'targets': [
    {
      'target_name': 'no_import_library',
      'type': 'loadable_module',
      'msvs_settings': {
        'NoImportLibrary': 'true',
      },
      'sources': ['dll_no_exports.cc'],
    },
  ]
}
