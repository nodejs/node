# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'default_installname',
      'type': 'shared_library',
      'sources': [ 'file.c' ],
    },
    {
      'target_name': 'default_bundle_installname',
      'product_name': 'My Framework',
      'type': 'shared_library',
      'mac_bundle': 1,
      'sources': [ 'file.c' ],
    },
    {
      'target_name': 'explicit_installname',
      'type': 'shared_library',
      'sources': [ 'file.c' ],
      'xcode_settings': {
        'LD_DYLIB_INSTALL_NAME': 'Trapped in a dynamiclib factory',
      },
    },
    {
      'target_name': 'explicit_installname_base',
      'type': 'shared_library',
      'sources': [ 'file.c' ],
      'xcode_settings': {
        'DYLIB_INSTALL_NAME_BASE': '@executable_path/../../..',

      },
    },
    {
      'target_name': 'explicit_installname_base_bundle',
      'product_name': 'My Other Framework',
      'type': 'shared_library',
      'mac_bundle': 1,
      'sources': [ 'file.c' ],
      'xcode_settings': {
        'DYLIB_INSTALL_NAME_BASE': '@executable_path/../../..',

      },
    },
    {
      'target_name': 'both_base_and_installname',
      'type': 'shared_library',
      'sources': [ 'file.c' ],
      'xcode_settings': {
        # LD_DYLIB_INSTALL_NAME wins.
        'LD_DYLIB_INSTALL_NAME': 'Still trapped in a dynamiclib factory',
        'DYLIB_INSTALL_NAME_BASE': '@executable_path/../../..',
      },
    },
    {
      'target_name': 'explicit_installname_with_base',
      'type': 'shared_library',
      'sources': [ 'file.c' ],
      'xcode_settings': {
        'LD_DYLIB_INSTALL_NAME': '$(DYLIB_INSTALL_NAME_BASE:standardizepath)/$(EXECUTABLE_PATH)',
      },
    },
    {
      'target_name': 'explicit_installname_with_explicit_base',
      'type': 'shared_library',
      'sources': [ 'file.c' ],
      'xcode_settings': {
        'DYLIB_INSTALL_NAME_BASE': '@executable_path/..',
        'LD_DYLIB_INSTALL_NAME': '$(DYLIB_INSTALL_NAME_BASE:standardizepath)/$(EXECUTABLE_PATH)',
      },
    },
    {
      'target_name': 'executable',
      'type': 'executable',
      'sources': [ 'main.c' ],
      'xcode_settings': {
        'LD_DYLIB_INSTALL_NAME': 'Should be ignored for not shared_lib',
      },
    },
    # Regression test for http://crbug.com/113918
    {
      'target_name': 'install_name_with_info_plist',
      'type': 'shared_library',
      'mac_bundle': 1,
      'sources': [ 'file.c' ],
      'xcode_settings': {
        'INFOPLIST_FILE': 'Info.plist',
        'LD_DYLIB_INSTALL_NAME': '$(DYLIB_INSTALL_NAME_BASE:standardizepath)/$(EXECUTABLE_PATH)',
      },
    },
  ],
}
