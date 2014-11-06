# Copyright (c) IBM Corporation and Others. All Rights Reserved.
# very loosely based on icu.gyp from Chromium:
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


{
  'variables': {
    'icu_src_derb': [ '../../deps/icu/source/tools/genrb/derb.c' ],
  },
  'includes': [ '../../icu_config.gypi' ],
  'targets': [
    {
      # a target to hold uconfig defines.
      # for now these are hard coded, but could be defined.
      'target_name': 'icu_uconfig',
      'type': 'none',
      'toolsets': [ 'host', 'target' ],
      'direct_dependent_settings': {
        'defines': [
          'UCONFIG_NO_LEGACY_CONVERSION=1',
          'UCONFIG_NO_IDNA=1',
          'UCONFIG_NO_TRANSLITERATION=1',
          'UCONFIG_NO_SERVICE=1',
          'UCONFIG_NO_REGULAR_EXPRESSIONS=1',
          'U_ENABLE_DYLOAD=0',
          'U_STATIC_IMPLEMENTATION=1',
          # Don't need std::string in API.
          # Also, problematic: <http://bugs.icu-project.org/trac/ticket/11333>
          'U_HAVE_STD_STRING=0',
          # TODO(srl295): reenable following pending
          # https://code.google.com/p/v8/issues/detail?id=3345
          # (saves some space)
          'UCONFIG_NO_BREAK_ITERATION=0',
        ],
      }
    },
    {
      # a target to hold common settings.
      # make any target that is ICU implementation depend on this.
      'target_name': 'icu_implementation',
      'toolsets': [ 'host', 'target' ],
      'type': 'none',
      'direct_dependent_settings': {
        'conditions': [
          [ 'os_posix == 1 and OS != "mac" and OS != "ios"', {
            'cflags': [ '-Wno-deprecated-declarations' ],
            'cflags_cc': [ '-frtti' ],
          }],
          [ 'OS == "mac" or OS == "ios"', {
            'xcode_settings': {'GCC_ENABLE_CPP_RTTI': 'YES' },
          }],
          [ 'OS == "win"', {
            'msvs_settings': {
              'VCCLCompilerTool': {'RuntimeTypeInfo': 'true'},
            }
          }],
        ],
        'msvs_settings': {
          'VCCLCompilerTool': {
            'RuntimeTypeInfo': 'true',
            'ExceptionHandling': '1',
          },
        },
        'configurations': {
          # TODO: why does this need to be redefined for Release and Debug?
          # Maybe this should be pushed into common.gypi with an "if v8 i18n"?
          'Release': {
            'msvs_settings': {
              'VCCLCompilerTool': {
                'RuntimeTypeInfo': 'true',
                'ExceptionHandling': '1',
              },
            },
          },
          'Debug': {
            'msvs_settings': {
              'VCCLCompilerTool': {
                'RuntimeTypeInfo': 'true',
                'ExceptionHandling': '1',
              },
            },
          },
        },
        'defines': [
          'U_ATTRIBUTE_DEPRECATED=',
          '_CRT_SECURE_NO_DEPRECATE=',
          'U_STATIC_IMPLEMENTATION=1',
        ],
      },
    },
    {
      'target_name': 'icui18n',
      'type': '<(library)',
      'toolsets': [ 'target' ],
      'sources': [
        '<@(icu_src_i18n)'
      ],
      'include_dirs': [
        '../../deps/icu/source/i18n',
      ],
      'defines': [
        'U_I18N_IMPLEMENTATION=1',
      ],
      'dependencies': [ 'icuucx', 'icu_implementation', 'icu_uconfig' ],
      'direct_dependent_settings': {
        'include_dirs': [
          '../../deps/icu/source/i18n',
        ],
      },
      'export_dependent_settings': [ 'icuucx' ],
    },
    # This exports actual ICU data
    {
      'target_name': 'icudata',
      'type': '<(library)',
      'toolsets': [ 'target' ],
      'conditions': [
        [ 'OS == "win"', {
          'conditions': [
            [ 'icu_small == "false"', { # and OS=win
              # full data - just build the full data file, then we are done.
              'sources': [ '<(SHARED_INTERMEDIATE_DIR)/icudt<(icu_ver_major)<(icu_endianness)_dat.obj' ],
              'dependencies': [ 'genccode#host' ],
              'actions': [
                {
                  'action_name': 'icudata',
                  'inputs': [ '<(icu_data_in)' ],
                  'outputs': [ '<(SHARED_INTERMEDIATE_DIR)/icudt<(icu_ver_major)<(icu_endianness)_dat.obj' ],
                  'action': [ '<(PRODUCT_DIR)/genccode',
                              '-o',
                              '-d', '<(SHARED_INTERMEDIATE_DIR)',
                              '-n', 'icudata',
                              '-e', 'icudt<(icu_ver_major)',
                              '<@(_inputs)' ],
                },
              ],
            }, { # icu_small == TRUE and OS == win
              # link against stub data primarily
              # then, use icupkg and genccode to rebuild data
              'dependencies': [ 'icustubdata', 'genccode#host', 'icupkg#host', 'genrb#host', 'iculslocs#host' ],
              'export_dependent_settings': [ 'icustubdata' ],
              'actions': [
                {
                  # trim down ICU
                  'action_name': 'icutrim',
                  'inputs': [ '<(icu_data_in)', 'icu_small.json' ],
                  'outputs': [ '../../out/icutmp/icudt<(icu_ver_major)<(icu_endianness).dat' ],
                  'action': [ 'python',
                              'icutrim.py',
                              '-P', '../../<(CONFIGURATION_NAME)',
                              '-D', '<(icu_data_in)',
                              '--delete-tmp',
                              '-T', '../../out/icutmp',
                              '-F', 'icu_small.json',
                              '-O', 'icudt<(icu_ver_major)<(icu_endianness).dat',
                              '-v' ],
                },
                {
                  # build final .dat -> .obj
                  'action_name': 'genccode',
                  'inputs': [ '../../out/icutmp/icudt<(icu_ver_major)<(icu_endianness).dat' ],
                  'outputs': [ '../../out/icudt<(icu_ver_major)<(icu_endianness)_dat.obj' ],
                  'action': [ '../../<(CONFIGURATION_NAME)/genccode',
                              '-o',
                              '-d', '../../out/',
                              '-n', 'icudata',
                              '-e', 'icusmdt<(icu_ver_major)',
                              '<@(_inputs)' ],
                },
              ],
              # This file contains the small ICU data.
              'sources': [ '../../out/icudt<(icu_ver_major)<(icu_endianness)_dat.obj' ],
            } ] ], #end of OS==win and icu_small == true
        }, { # OS != win
          'conditions': [
            [ 'icu_small == "false"', {
              # full data - just build the full data file, then we are done.
              'sources': [ '<(SHARED_INTERMEDIATE_DIR)/icudt<(icu_ver_major)_dat.c' ],
              'dependencies': [ 'genccode#host', 'icupkg#host', 'icu_implementation#host', 'icu_uconfig' ],
              'include_dirs': [
                '../../deps/icu/source/common',
              ],
              'actions': [
                {
                   # Swap endianness (if needed), or at least copy the file
                   'action_name': 'icupkg',
                   'inputs': [ '<(icu_data_in)' ],
                   'outputs':[ '<(SHARED_INTERMEDIATE_DIR)/icudt<(icu_ver_major)<(icu_endianness).dat' ],
                   'action': [ '<(PRODUCT_DIR)/icupkg',
                               '-t<(icu_endianness)',
                               '<@(_inputs)',
                               '<@(_outputs)',
                             ],
                },
                {
                   # Rename without the endianness marker
                   'action_name': 'copy',
                   'inputs': [ '<(SHARED_INTERMEDIATE_DIR)/icudt<(icu_ver_major)<(icu_endianness).dat' ],
                   'outputs':[ '<(SHARED_INTERMEDIATE_DIR)/icudt<(icu_ver_major).dat' ],
                   'action': [ 'cp',
                               '<@(_inputs)',
                               '<@(_outputs)',
                             ],
                },
                {
                  'action_name': 'icudata',
                  'inputs': [ '<(SHARED_INTERMEDIATE_DIR)/icudt<(icu_ver_major).dat' ],
                  'outputs':[ '<(SHARED_INTERMEDIATE_DIR)/icudt<(icu_ver_major)_dat.c' ],
                  'action': [ '<(PRODUCT_DIR)/genccode',
                              '-e', 'icudt<(icu_ver_major)',
                              '-d', '<(SHARED_INTERMEDIATE_DIR)',
                              '-f', 'icudt<(icu_ver_major)_dat',
                              '<@(_inputs)' ],
                },
              ], # end actions
            }, { # icu_small == true ( and OS != win )
              # link against stub data (as primary data)
              # then, use icupkg and genccode to rebuild small data
              'dependencies': [ 'icustubdata', 'genccode#host', 'icupkg#host', 'genrb#host', 'iculslocs#host',
                               'icu_implementation', 'icu_uconfig' ],
              'export_dependent_settings': [ 'icustubdata' ],
              'actions': [
                {
                  # trim down ICU
                  'action_name': 'icutrim',
                  'inputs': [ '<(icu_data_in)', 'icu_small.json' ],
                  'outputs': [ '<(SHARED_INTERMEDIATE_DIR)/icutmp/icudt<(icu_ver_major)<(icu_endianness).dat' ],
                  'action': [ 'python',
                              'icutrim.py',
                              '-P', '<(PRODUCT_DIR)',
                              '-D', '<(icu_data_in)',
                              '--delete-tmp',
                              '-T', '<(SHARED_INTERMEDIATE_DIR)/icutmp',
                              '-F', 'icu_small.json',
                              '-O', 'icudt<(icu_ver_major)<(icu_endianness).dat',
                              '-v' ],
                }, {
                  # rename to get the final entrypoint name right
                   'action_name': 'rename',
                   'inputs': [ '<(SHARED_INTERMEDIATE_DIR)/icutmp/icudt<(icu_ver_major)<(icu_endianness).dat' ],
                   'outputs': [ '<(SHARED_INTERMEDIATE_DIR)/icutmp/icusmdt<(icu_ver_major).dat' ],
                   'action': [ 'cp',
                               '<@(_inputs)',
                               '<@(_outputs)',
                             ],
                }, {
                  # build final .dat -> .obj
                  'action_name': 'genccode',
                  'inputs': [ '<(SHARED_INTERMEDIATE_DIR)/icutmp/icusmdt<(icu_ver_major).dat' ],
                  'outputs': [ '<(SHARED_INTERMEDIATE_DIR)/icusmdt<(icu_ver_major)_dat.c' ],
                  'action': [ '<(PRODUCT_DIR)/genccode',
                              '-d', '<(SHARED_INTERMEDIATE_DIR)',
                              '<@(_inputs)' ],
                },
              ],
              # This file contains the small ICU data
              'sources': [ '<(SHARED_INTERMEDIATE_DIR)/icusmdt<(icu_ver_major)_dat.c' ],
              # for umachine.h
              'include_dirs': [
                '../../deps/icu/source/common',
              ],
            }]], # end icu_small == true
        }]], # end OS != win
    }, # end icudata
    # icustubdata is a tiny (~1k) symbol with no ICU data in it.
    # tools must link against it as they are generating the full data.
    {
      'target_name': 'icustubdata',
      'type': '<(library)',
      'toolsets': [ 'target' ],
      'dependencies': [ 'icu_implementation' ],
      'sources': [
        '<@(icu_src_stubdata)'
      ],
      'include_dirs': [
        '../../deps/icu/source/common',
      ],
    },
    # this target is for v8 consumption.
    # it is icuuc + stubdata
    # it is only built for target
    {
      'target_name': 'icuuc',
      'type': 'none',
      'toolsets': [ 'target' ],
      'dependencies': [ 'icuucx', 'icudata' ],
      'export_dependent_settings': [ 'icuucx', 'icudata' ],
    },
    # This is the 'real' icuuc.
    # tools can depend on 'icuuc + stubdata'
    {
      'target_name': 'icuucx',
      'type': '<(library)',
      'dependencies': [ 'icu_implementation', 'icu_uconfig' ],
      'toolsets': [ 'target' ],
      'sources': [
        '<@(icu_src_common)'
      ],
      'include_dirs': [
        '../../deps/icu/source/common',
      ],
      'defines': [
        'U_COMMON_IMPLEMENTATION=1',
      ],
      'export_dependent_settings': [ 'icu_uconfig' ],
      'direct_dependent_settings': {
        'include_dirs': [
          '../../deps/icu/source/common',
        ],
        'conditions': [
          [ 'OS=="win"', {
            'link_settings': {
              'libraries': [ '-lAdvAPI32.Lib', '-lUser32.lib' ],
            },
          }],
        ],
      },
    },
    # tools library
    {
      'target_name': 'icutools',
      'type': '<(library)',
      'toolsets': [ 'host' ],
      'dependencies': [ 'icu_implementation', 'icu_uconfig' ],
      'sources': [
        '<@(icu_src_tools)',
        '<@(icu_src_common)',
        '<@(icu_src_i18n)',
        '<@(icu_src_io)',
        '<@(icu_src_stubdata)',
      ],
      'include_dirs': [
        '../../deps/icu/source/common',
        '../../deps/icu/source/i18n',
        '../../deps/icu/source/io',
        '../../deps/icu/source/tools/toolutil',
      ],
      'defines': [
        'U_COMMON_IMPLEMENTATION=1',
        'U_I18N_IMPLEMENTATION=1',
        'U_IO_IMPLEMENTATION=1',
        'U_TOOLUTIL_IMPLEMENTATION=1',
        #'DEBUG=0', # http://bugs.icu-project.org/trac/ticket/10977
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '../../deps/icu/source/common',
          '../../deps/icu/source/i18n',
          '../../deps/icu/source/io',
          '../../deps/icu/source/tools/toolutil',
        ],
        'conditions': [
          [ 'OS=="win"', {
            'link_settings': {
              'libraries': [ '-lAdvAPI32.Lib', '-lUser32.lib' ],
            },
          }],
        ],
      },
      'export_dependent_settings': [ 'icu_implementation', 'icu_uconfig' ],
    },
    # This tool is needed to rebuild .res files from .txt,
    # or to build index (res_index.txt) files for small-icu
    {
      'target_name': 'genrb',
      'type': 'executable',
      'toolsets': [ 'host' ],
      'dependencies': [ 'icutools' ],
      'sources': [
        '<@(icu_src_genrb)'
      ],
      # derb is a separate executable
      # (which is not currently built)
      'sources!': [
        '<@(icu_src_derb)',
        'no-op.cc',
      ],
    },
    # This tool is used to rebuild res_index.res manifests
    {
      'target_name': 'iculslocs',
      'toolsets': [ 'host' ],
      'type': 'executable',
      'dependencies': [ 'icutools' ],
      'sources': [
        'iculslocs.cc',
        'no-op.cc',
      ],
    },
    # This tool is used to package, unpackage, repackage .dat files
    # and convert endianesses
    {
      'target_name': 'icupkg',
      'toolsets': [ 'host' ],
      'type': 'executable',
      'dependencies': [ 'icutools' ],
      'sources': [
        '<@(icu_src_icupkg)',
        'no-op.cc',
      ],
    },
    # this is used to convert .dat directly into .obj
    {
      'target_name': 'genccode',
      'toolsets': [ 'host' ],
      'type': 'executable',
      'dependencies': [ 'icutools' ],
      'sources': [
        '<@(icu_src_genccode)',
        'no-op.cc',
      ],
    },
  ],
}
