# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'raw',
      'type': 'shared_library',
      'sources': [ 'file.c', ],
      'xcode_settings': {
        'OTHER_LDFLAGS': [
          '-exported_symbols_list symbol_list.def',
          '-sectcreate __TEXT __info_plist Info.plist',
        ],
      },
    },
    # TODO(thakis): This form should ideally be supported, too. (But
    # -Wlfoo,bar,baz is cleaner so people should use that anyway.)
    #{
    #  'target_name': 'raw_sep',
    #  'type': 'shared_library',
    #  'sources': [ 'file.c', ],
    #  'xcode_settings': {
    #    'OTHER_LDFLAGS': [
    #      '-exported_symbols_list', 'symbol_list.def',
    #      '-sectcreate', '__TEXT', '__info_plist', 'Info.plist',
    #    ],
    #  },
    #},
    {
      'target_name': 'wl_space',
      'type': 'shared_library',
      'sources': [ 'file.c', ],
      'xcode_settings': {
        'OTHER_LDFLAGS': [
          # Works because clang passes unknown files on to the linker.
          '-Wl,-exported_symbols_list symbol_list.def',
        ],
      },
    },
    # TODO(thakis): This form should ideally be supported, too. (But
    # -Wlfoo,bar,baz is cleaner so people should use that anyway.)
    #{
    #  'target_name': 'wl_space_sep',
    #  'type': 'shared_library',
    #  'sources': [ 'file.c', ],
    #  'xcode_settings': {
    #    'OTHER_LDFLAGS': [
    #      # Works because clang passes unknown files on to the linker.
    #      '-Wl,-exported_symbols_list', 'symbol_list.def',
    #    ],
    #  },
    #},
    {
      'target_name': 'wl_comma',
      'type': 'shared_library',
      'sources': [ 'file.c', ],
      'xcode_settings': {
        'OTHER_LDFLAGS': [
          '-Wl,-exported_symbols_list,symbol_list.def',
          '-Wl,-sectcreate,__TEXT,__info_plist,Info.plist',
        ],
      },
    },
  ],
}
