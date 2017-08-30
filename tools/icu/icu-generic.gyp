# Copyright (c) IBM Corporation and Others. All Rights Reserved.
# very loosely based on icu.gyp from Chromium:
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


{
  'variables': {
    'icu_src_derb': [
      '<(icu_path)/source/tools/genrb/derb.c',
      '<(icu_path)/source/tools/genrb/derb.cpp'
    ],
  },
  'includes': [ '../../icu_config.gypi' ],
  'targets': [
    {
      # a target for additional uconfig defines, target only
      'target_name': 'icu_uconfig_target',
      'type': 'none',
      'toolsets': [ 'target' ],
      'direct_dependent_settings': {
        'defines': []
      },
    },
    {
      # a target to hold uconfig defines.
      # for now these are hard coded, but could be defined.
      'target_name': 'icu_uconfig',
      'type': 'none',
      'toolsets': [ 'host', 'target' ],
      'direct_dependent_settings': {
        'defines': [
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
            'cflags_cc!': [ '-fno-rtti' ],
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
      'toolsets': [ 'target', 'host' ],
      'conditions' : [
        ['_toolset=="target"', {
          'type': '<(library)',
          'sources': [
            '<@(icu_src_i18n)'
          ],
          ## if your compiler can dead-strip, these exclusions will
          ## make ZERO difference to binary size.
          ## Made ICU-specific for future-proofing.
          'conditions': [
            [ 'icu_ver_major == 55', { 'sources!': [
              # alphabetic index
              '<(icu_path)/source/i18n/alphaindex.cpp',
              # BOCSU
              # misc
              '<(icu_path)/source/i18n/regexcmp.cpp',
              '<(icu_path)/source/i18n/regexcmp.h',
              '<(icu_path)/source/i18n/regexcst.h',
              '<(icu_path)/source/i18n/regeximp.cpp',
              '<(icu_path)/source/i18n/regeximp.h',
              '<(icu_path)/source/i18n/regexst.cpp',
              '<(icu_path)/source/i18n/regexst.h',
              '<(icu_path)/source/i18n/regextxt.cpp',
              '<(icu_path)/source/i18n/regextxt.h',
              '<(icu_path)/source/i18n/region.cpp',
              '<(icu_path)/source/i18n/region_impl.h',
              '<(icu_path)/source/i18n/reldatefmt.cpp',
              '<(icu_path)/source/i18n/reldatefmt.h'
              '<(icu_path)/source/i18n/scientificformathelper.cpp',
              '<(icu_path)/source/i18n/tmunit.cpp',
              '<(icu_path)/source/i18n/tmutamt.cpp',
              '<(icu_path)/source/i18n/tmutfmt.cpp',
              '<(icu_path)/source/i18n/uregex.cpp',
              '<(icu_path)/source/i18n/uregexc.cpp',
              '<(icu_path)/source/i18n/uregion.cpp',
              '<(icu_path)/source/i18n/uspoof.cpp',
              '<(icu_path)/source/i18n/uspoof_build.cpp',
              '<(icu_path)/source/i18n/uspoof_conf.cpp',
              '<(icu_path)/source/i18n/uspoof_conf.h',
              '<(icu_path)/source/i18n/uspoof_impl.cpp',
              '<(icu_path)/source/i18n/uspoof_impl.h',
              '<(icu_path)/source/i18n/uspoof_wsconf.cpp',
              '<(icu_path)/source/i18n/uspoof_wsconf.h',
            ]}],
            [ 'icu_ver_major == 57', { 'sources!': [

              # alphabetic index
              '<(icu_path)/source/i18n/alphaindex.cpp',
              # BOCSU
              # misc
              '<(icu_path)/source/i18n/regexcmp.cpp',
              '<(icu_path)/source/i18n/regexcmp.h',
              '<(icu_path)/source/i18n/regexcst.h',
              '<(icu_path)/source/i18n/regeximp.cpp',
              '<(icu_path)/source/i18n/regeximp.h',
              '<(icu_path)/source/i18n/regexst.cpp',
              '<(icu_path)/source/i18n/regexst.h',
              '<(icu_path)/source/i18n/regextxt.cpp',
              '<(icu_path)/source/i18n/regextxt.h',
              '<(icu_path)/source/i18n/region.cpp',
              '<(icu_path)/source/i18n/region_impl.h',
              '<(icu_path)/source/i18n/reldatefmt.cpp',
              '<(icu_path)/source/i18n/reldatefmt.h'
              '<(icu_path)/source/i18n/scientificformathelper.cpp',
              '<(icu_path)/source/i18n/tmunit.cpp',
              '<(icu_path)/source/i18n/tmutamt.cpp',
              '<(icu_path)/source/i18n/tmutfmt.cpp',
              '<(icu_path)/source/i18n/uregex.cpp',
              '<(icu_path)/source/i18n/uregexc.cpp',
              '<(icu_path)/source/i18n/uregion.cpp',
              '<(icu_path)/source/i18n/uspoof.cpp',
              '<(icu_path)/source/i18n/uspoof_build.cpp',
              '<(icu_path)/source/i18n/uspoof_conf.cpp',
              '<(icu_path)/source/i18n/uspoof_conf.h',
              '<(icu_path)/source/i18n/uspoof_impl.cpp',
              '<(icu_path)/source/i18n/uspoof_impl.h',
              '<(icu_path)/source/i18n/uspoof_wsconf.cpp',
              '<(icu_path)/source/i18n/uspoof_wsconf.h',
            ]}],
            ],
          'include_dirs': [
            '<(icu_path)/source/i18n',
          ],
          'defines': [
            'U_I18N_IMPLEMENTATION=1',
          ],
          'dependencies': [ 'icuucx', 'icu_implementation', 'icu_uconfig', 'icu_uconfig_target' ],
          'direct_dependent_settings': {
            'include_dirs': [
              '<(icu_path)/source/i18n',
            ],
          },
          'export_dependent_settings': [ 'icuucx', 'icu_uconfig_target' ],
        }],
        ['_toolset=="host"', {
          'type': 'none',
          'dependencies': [ 'icutools' ],
          'export_dependent_settings': [ 'icutools' ],
        }],
      ],
    },
    # This exports actual ICU data
    {
      'target_name': 'icudata',
      'type': '<(library)',
      'toolsets': [ 'target' ],
      'include_dirs': ['<(icu_path)/source/common', ],
      'export_dependent_settings': ['icustubdata'],

      # ============== logic flow ===============
      # so we have a 2x2 matrix: win +/- vs. icu_small +/-
      # 1 - For small+ we trim the pre-build dat '<(icu_data_in)'
      # 2 - For win- && small- we need we need to repack '<(icu_data_in)' for endiness
      # 3 - everybody likes the <(icu_symbol_name) different,
      #     and for some reason are sensitive to <(icudata_genccode_input)
      
      # ============== Gattchas ==============
      # 1 - genccode can only output .c files on non-windows
      #     but it can directly compile to .o file on windows.
      # 2 - need to repack for endiness only on non-windows
      # 3 - '<(PRODUCT_DIR)/.' the '/.' suffix is so that GYP will not to make
      #     a mess of this argument is the case it ends a backslash `\`
      # 4 - `--delete-tmp` deletes the whole content of the directory -T points
      #     to before it operates, so we need a special <(icudata_trim_temp)
      #     directory and then copy/rename the files back out
      
      # ============== refack comment ==============
      # IMHO the simplest/robust way was to parameterize the tasks, and set the 
      # parameters in a 4 clause condition. This way I was able to make the GYP 
      # output stay as close as I could to the previus state
      
      # P.S./TODO the situation where the `.gyp` file is in `tools/icu` while
      # the code is in `deps/` makes it cumbersome for a human to reason about


      'variables': {
        'icu_output_prefix': 'icudt<(icu_ver_major)',
        'icu_output_prefix_sm': 'icusmdt<(icu_ver_major)',
        'icudata_trim_temp': '<(SHARED_INTERMEDIATE_DIR)/icutmp',
        'icudata_trim_name': '<(icu_output_prefix)<(icu_endianness).dat',
        'icudata_trim_output': '<(icudata_trim_temp)/>(icudata_trim_name)',
        'icudata_output_name_win': '>(icu_output_prefix)<(icu_endianness)_dat.obj',
        'icudata_output_name_psx': '>(icu_symbol_name)_dat.c',
        'icudata_genccode_extra_args': [ '-o', '-n', 'icudata', '-e', '>(icu_symbol_name)' ],
        'icudata_genccode_output': '<(SHARED_INTERMEDIATE_DIR)/>(icudata_genccode_output_name)',
      },
      'dependencies': [ 'genccode#host', 'icupkg#host', 'genrb#host',
                        'iculslocs#host', 'icu_implementation', 'icustubdata',
                        'icu_uconfig' ],
      'sources': ['<(icudata_genccode_output)'],

      'conditions': [
        ['OS == "win" and icu_small == "true"',
          {
            'variables': {
              'icu_symbol_name': '<(icu_output_prefix_sm)',
              'icudata_genccode_output_name': '<(icudata_output_name_win)',
              'icudata_genccode_input': '<(SHARED_INTERMEDIATE_DIR)/>(icudata_trim_name)',
            }
          }
        ],
        ['OS == "win" and icu_small == "false"',
          {
            'variables': {
              'icu_symbol_name': '<(icu_output_prefix)',
              'icudata_genccode_output_name': '<(icudata_output_name_win)',
              # for windows with full-icu, genccode runs directly on input
              'icudata_genccode_input': '<(icu_data_in)',
            }
          }
        ],
        ['OS != "win" and icu_small == "true"',
          {
            'variables': {
              'icu_symbol_name': '<(icu_output_prefix_sm)',
              'icudata_genccode_output_name': '<(icudata_output_name_psx)',
              'icudata_genccode_extra_args=': [ ],
              'icudata_genccode_input': '<(icudata_trim_temp)/>(icu_symbol_name).dat',
           }
          }
        ],
        ['OS != "win" and icu_small == "false"',
          {
            'variables': {
              'icu_symbol_name': '<(icu_output_prefix)',
              'icudata_genccode_output_name': '<(icudata_output_name_psx)',
              'icudata_genccode_extra_args=': [ '-f', '>(icu_symbol_name)_dat', '-e', '>(icu_symbol_name)' ],
              'icudata_genccode_input': '<(SHARED_INTERMEDIATE_DIR)/>(icu_symbol_name).dat'
            },
            'actions': [
              {
                # Swap endianness (if needed), or at least rename the file
                'action_name': 'icudata-pkg',
                'inputs': [ '<(icu_data_in)' ],
                'outputs':[ '>(icudata_genccode_input)' ],
                'action': [ '<(PRODUCT_DIR)/icupkg',
                            '-t<(icu_endianness)',
                            '<@(_inputs)',
                            '<@(_outputs)' ],
              }
            ]
          }
        ],
        # if icu_small == "true" we trim
        ['icu_small == "true"', {
          'actions': [
            {
              'action_name': 'icudata-trim',
              'msvs_quote_cmd': 0,
              'inputs': [ '<(icu_data_in)', 'icu_small.json' ],
              # icudata_trim_output = icudata_trim_temp (-T) + icudata_trim_name (-O)
              'outputs': [ '<(icudata_trim_output)' ],
              'action': [ 'python', 'icutrim.py',
                          '-P', '<(PRODUCT_DIR)/.', # '.' suffix is a workaround against GYP assumptions :(
                          '-D', '<(icu_data_in)',
                          '--delete-tmp',
                          '-T', '<(icudata_trim_temp)',
                          '-F', 'icu_small.json',
                          '-O', '>(icudata_trim_name)',
                          '-v',
                          '-L', '<(icu_locales)'],
            }, {
              'action_name': 'icudata-trim-copy',
              'msvs_quote_cmd': 0,
              'inputs': ['<(icudata_trim_output)'],
              'outputs': ['>(icudata_genccode_input)'],
              'action': ['cp', '<@(_inputs)', '<@(_outputs)'],
            }
          ],
        }],
        [ '1==1', { # using '1' just for symmetry
          # full data - just build the full data file, then we are done.
          # depending on `icu_small` flag, we chose the naming, and the args
          'actions': [
            {
              'action_name': 'icudata-genccode',
              'msvs_quote_cmd': 0,
              'inputs': [ '>(icudata_genccode_input)' ],
              'outputs':[ '<(icudata_genccode_output)' ],
              'action': [ '<(PRODUCT_DIR)/genccode',
                          '-d', '<(SHARED_INTERMEDIATE_DIR)',
                          '>@(icudata_genccode_extra_args)',
                          '<@(_inputs)' ],
            },
          ],
        } ],
      ], # end conditions
    }, # end target_name: icudata


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
        '<(icu_path)/source/common',
      ],
    },
    # this target is for v8 consumption.
    # it is icuuc + stubdata
    # it is only built for target
    {
      'target_name': 'icuuc',
      'type': 'none',
      'toolsets': [ 'target', 'host' ],
      'conditions' : [
        ['_toolset=="host"', {
          'dependencies': [ 'icutools' ],
          'export_dependent_settings': [ 'icutools' ],
        }],
        ['_toolset=="target"', {
          'dependencies': [ 'icuucx', 'icudata' ],
          'export_dependent_settings': [ 'icuucx', 'icudata' ],
        }],
      ],
    },
    # This is the 'real' icuuc.
    {
      'target_name': 'icuucx',
      'type': '<(library)',
      'dependencies': [ 'icu_implementation', 'icu_uconfig', 'icu_uconfig_target' ],
      'toolsets': [ 'target' ],
      'sources': [
        '<@(icu_src_common)',
      ],
          ## if your compiler can dead-strip, this will
          ## make ZERO difference to binary size.
          ## Made ICU-specific for future-proofing.
      'conditions': [
        [ 'icu_ver_major == 55', { 'sources!': [

          # bidi- not needed (yet!)
          '<(icu_path)/source/common/ubidi.c',
          '<(icu_path)/source/common/ubidiimp.h',
          '<(icu_path)/source/common/ubidiln.c',
          '<(icu_path)/source/common/ubidiwrt.c',
          #'<(icu_path)/source/common/ubidi_props.c',
          #'<(icu_path)/source/common/ubidi_props.h',
          #'<(icu_path)/source/common/ubidi_props_data.h',
          # and the callers
          '<(icu_path)/source/common/ushape.cpp',
        ]}],
        [ 'icu_ver_major == 57', { 'sources!': [
          # work around http://bugs.icu-project.org/trac/ticket/12451
          # (benign afterwards)
          '<(icu_path)/source/common/cstr.cpp',

          # bidi- not needed (yet!)
          '<(icu_path)/source/common/ubidi.c',
          '<(icu_path)/source/common/ubidiimp.h',
          '<(icu_path)/source/common/ubidiln.c',
          '<(icu_path)/source/common/ubidiwrt.c',
          #'<(icu_path)/source/common/ubidi_props.c',
          #'<(icu_path)/source/common/ubidi_props.h',
          #'<(icu_path)/source/common/ubidi_props_data.h',
          # and the callers
          '<(icu_path)/source/common/ushape.cpp',
        ]}],
        [ 'OS == "solaris"', { 'defines': [
          '_XOPEN_SOURCE_EXTENDED=0',
        ]}],
      ],
      'include_dirs': [
        '<(icu_path)/source/common',
      ],
      'defines': [
        'U_COMMON_IMPLEMENTATION=1',
      ],
      'cflags_c': ['-std=c99'],
      'export_dependent_settings': [ 'icu_uconfig', 'icu_uconfig_target' ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(icu_path)/source/common',
        ],
        'conditions': [
          [ 'OS=="win"', {
            'link_settings': {
              'libraries': [ '-lAdvAPI32.lib', '-lUser32.lib' ],
            },
          }],
        ],
      },
    },
    # tools library. This builds all of ICU together.
    {
      'target_name': 'icutools',
      'type': '<(library)',
      'toolsets': [ 'host' ],
      'dependencies': [ 'icu_implementation', 'icu_uconfig' ],
      'sources': [
        '<@(icu_src_tools)',
        '<@(icu_src_common)',
        '<@(icu_src_i18n)',
        '<@(icu_src_stubdata)',
      ],
      'sources!': [
        '<(icu_path)/source/tools/toolutil/udbgutil.cpp',
        '<(icu_path)/source/tools/toolutil/udbgutil.h',
        '<(icu_path)/source/tools/toolutil/dbgutil.cpp',
        '<(icu_path)/source/tools/toolutil/dbgutil.h',
      ],
      'include_dirs': [
        '<(icu_path)/source/common',
        '<(icu_path)/source/i18n',
        '<(icu_path)/source/tools/toolutil',
      ],
      'defines': [
        'U_COMMON_IMPLEMENTATION=1',
        'U_I18N_IMPLEMENTATION=1',
        'U_IO_IMPLEMENTATION=1',
        'U_TOOLUTIL_IMPLEMENTATION=1',
        #'DEBUG=0', # http://bugs.icu-project.org/trac/ticket/10977
      ],
      'cflags_c': ['-std=c99'],
      'conditions': [
        ['OS == "solaris"', {
          'defines': [ '_XOPEN_SOURCE_EXTENDED=0' ]
        }]
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(icu_path)/source/common',
          '<(icu_path)/source/i18n',
          '<(icu_path)/source/tools/toolutil',
        ],
        'conditions': [
          [ 'OS=="win"', {
            'link_settings': {
              'libraries': [ '-lAdvAPI32.lib', '-lUser32.lib' ],
            },
          }],
        ],
      },
      'export_dependent_settings': [ 'icu_uconfig' ],
    },


    # ====================================
    # Helper tools
    # ====================================

    # rebuild .res files from .txt, or build index (res_index.txt) for small-icu
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

    # rebuild res_index.res manifests
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

    # package, unpackage, repackage .dat files, and convert endianesses
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

    # convert .dat directly into .obj
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
