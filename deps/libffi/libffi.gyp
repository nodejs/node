# This file is used with the GYP meta build system.
# http://code.google.com/p/gyp
# To build try this:
#   svn co http://gyp.googlecode.com/svn/trunk gyp
#   ./gyp/gyp -f make --depth=`pwd` libffi.gyp
#   make
#   ./out/Debug/test

{
  'variables': {
    'target_arch%': 'ia32', # built for a 32-bit CPU by default
  },
  'target_defaults': {
    'default_configuration': 'Debug',
    'configurations': {
      # TODO: hoist these out and put them somewhere common, because
      #       RuntimeLibrary MUST MATCH across the entire project
      'Debug': {
        'defines': [ 'DEBUG', '_DEBUG' ],
        'msvs_settings': {
          'VCCLCompilerTool': {
            'RuntimeLibrary': 1, # static debug
          },
        },
      },
      'Release': {
        'defines': [ 'NDEBUG' ],
        'msvs_settings': {
          'VCCLCompilerTool': {
            'RuntimeLibrary': 0, # static release
          },
        },
      }
    },
    'msvs_settings': {
      'VCCLCompilerTool': {
      },
      'VCLibrarianTool': {
      },
      'VCLinkerTool': {
        'GenerateDebugInformation': 'true',
      },
    },
    'conditions': [
      ['OS == "win"', {
        'defines': [
          'WIN32'
        ],
      }]
    ],
  },

  # Compile .S files on Windows
  'conditions': [
    ['OS=="win"', {
      'target_defaults': {
        'conditions': [
          ['target_arch=="ia32"', {
            'variables': { 'ml': ['ml', '/c', '/nologo', '/safeseh' ] }
          }, 'target_arch=="arm64"', {
            'variables': { 'ml': ['armasm64', '/nologo' ] }
          }, {
            'variables': { 'ml': ['ml64', '/c', '/nologo' ] }
          }]
        ],
        'rules': [
          {
            'rule_name': 'preprocess_asm',
            'msvs_cygwin_shell': 0,
            'extension': 'preasm',
            'inputs': [
            ],
            'outputs': [
              '<(INTERMEDIATE_DIR)/<(RULE_INPUT_ROOT).asm',
            ],
            'action': [
              'call',
              'preprocess_asm.cmd',
                'include',
                'config/<(OS)/<(target_arch)',
                '<(RULE_INPUT_PATH)',
                '<(INTERMEDIATE_DIR)/<(RULE_INPUT_ROOT).asm',
            ],
            'message': 'Preprocessing assembly file <(RULE_INPUT_PATH)',
            'process_outputs_as_sources': 1,
          },
          {
            'rule_name': 'build_asm',
            'msvs_cygwin_shell': 0,
            'extension': 'asm',
            'inputs': [
            ],
            'outputs': [
              '<(INTERMEDIATE_DIR)/<(RULE_INPUT_ROOT).obj',
            ],
            'action': [
              '<@(ml)', '/Fo<(INTERMEDIATE_DIR)/<(RULE_INPUT_ROOT).obj',
                '<(INTERMEDIATE_DIR)/<(RULE_INPUT_ROOT).asm',
            ],
            'message': 'Building assembly file <(RULE_INPUT_PATH)',
            'process_outputs_as_sources': 1,
          },
        ],
      },
    }],
  ],

  'targets': [
    {
      'target_name': 'libffi',
      'type': 'static_library',

      # for CentOS 5 support: https://github.com/rbranson/node-ffi/issues/110
      'standalone_static_library': 1,

      'sources': [
        'src/prep_cif.c',
        'src/types.c',
        'src/raw_api.c',
        'src/java_raw_api.c',
        'src/closures.c',
      ],
      'defines': [
        'PIC',
        'FFI_BUILDING',
        'HAVE_CONFIG_H'
      ],
      'include_dirs': [
        'include',
        # platform and arch-specific headers
        'config/<(OS)/<(target_arch)'
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'include',
          # platform and arch-specific headers
          'config/<(OS)/<(target_arch)'
        ],
      },
      'conditions': [
        ['target_arch=="arm"', {
          'sources': [ 'src/arm/ffi.c' ],
          'conditions': [
            ['OS=="linux"', {
              'sources': [ 'src/arm/sysv.S' ]
            }]
          ]
        },'target_arch=="arm64"', {
          'sources': [ 'src/aarch64/ffi.c' ],
          'conditions': [
            ['OS=="linux" or OS=="mac"', {
              'sources': [ 'src/aarch64/sysv.S' ]
            }],
            ['OS=="win"', {
              'sources': [ 'src/aarch64/win64_armasm.preasm' ]
            }],
          ]
        }, { # ia32 or x64
          'conditions': [
            ['target_arch=="ia32"', {
              'sources': [ 'src/x86/ffi.c' ],
              'conditions': [
                ['OS=="win"', {
                  'sources': [ 'src/x86/sysv_intel.preasm' ],
                }, {
                  'sources': [ 'src/x86/sysv.S' ],
                }],
              ],
            }],
            ['target_arch=="x64"', {
              'sources': [
                'src/x86/ffiw64.c',
              ],
              'conditions': [
                ['OS=="win"', {
                  'sources': [
                    'src/x86/win64_intel.preasm',
                  ],
                }, {
                  'sources': [
                    'src/x86/ffi64.c',
                    'src/x86/unix64.S',
                    'src/x86/win64.S',
                  ],
                }]
              ],
            }],
            ['target_arch=="s390x"', {
              'sources': [
                'src/s390/ffi.c',
                'src/s390/sysv.S',
              ],
            }],
            ['OS=="win"', {
              # the libffi dlmalloc.c file has a bunch of implicit conversion
              # warnings, and the main ffi.c file contains one, so silence them
              'msvs_disabled_warnings': [ 4267 ],
            }],
          ]
        }],
      ]
    },
  ]
}
