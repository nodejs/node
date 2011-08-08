{
  'variables': {
    'library%': 'static_library',
    'component%': 'static_library',
    'visibility%': 'hidden',
    'variables': {
      'conditions': [
        [ 'OS=="linux" or OS=="freebsd" or OS=="openbsd"', {
          # This handles the Linux platforms we generally deal with. Anything
          # else gets passed through, which probably won't work very well; such
          # hosts should pass an explicit target_arch to gyp.
          'host_arch%':
            '<!(uname -m | sed -e "s/i.86/ia32/;s/x86_64/x64/;s/amd64/x64/;s/arm.*/arm/")',
        }, {  # OS!="linux" and OS!="freebsd" and OS!="openbsd"
          'host_arch%': 'ia32',
        }],
      ],
    },
    'host_arch%': '<(host_arch)',
    'target_arch%': '<(host_arch)',
    'v8_target_arch%': '<(target_arch)',
  },
  'target_defaults': {
    'default_configuration': 'Debug',
    'configurations': {
      'Debug': {
        'cflags': [ '-g', '-O0' ],
        'defines': [ '_DEBUG', 'DEBUG' ],
      },
      'Release': {
        'cflags': [ '-O3', '-fomit-frame-pointer', '-fdata-sections', '-ffunction-sections' ],
      },
    },
  },
  'conditions': [
    [ 'OS=="linux" or OS=="freebsd" or OS=="openbsd" or OS=="solaris"', {
      'target_defaults': {
        'cflags': [ '-Wall', '-pthread', '-fno-rtti', '-fno-exceptions' ],
        'ldflags': [ '-pthread', ],
        'conditions': [
          [ 'target_arch=="ia32"', {
            'cflags': [ '-m32' ],
            'ldflags': [ '-m32' ],
          }],
          [ 'OS=="linux"', {
            'cflags': [ '-ansi' ],
          }],
          [ 'visibility=="hidden"', {
            'cflags': [ '-fvisibility=hidden' ],
          }],
        ],
      },
    }],  # 'OS=="linux" or OS=="freebsd" or OS=="openbsd" or OS=="solaris"'
    ['OS=="win"', {
      'target_defaults': {
        'defines': [
          'WIN32',
          '_CRT_SECURE_NO_DEPRECATE',
          '_CRT_NONSTDC_NO_DEPRECATE',
        ],
        'conditions': [
          ['component=="static_library"', {
            'defines': [
              '_HAS_EXCEPTIONS=0',
            ],
          }],
        ],
        'msvs_disabled_warnings': [4355, 4800],
        'msvs_settings': {
          'VCCLCompilerTool': {
            'MinimalRebuild': 'false',
            'BufferSecurityCheck': 'true',
            'EnableFunctionLevelLinking': 'true',
            'RuntimeTypeInfo': 'false',
            'WarningLevel': '3',
            'WarnAsError': 'true',
            'DebugInformationFormat': '3',
            'Detect64BitPortabilityProblems': 'false',
            'conditions': [
              [ 'msvs_multi_core_compile', {
                'AdditionalOptions': ['/MP'],
              }],
              ['component=="shared_library"', {
                'ExceptionHandling': '1',  # /EHsc
              }, {
                'ExceptionHandling': '0',
              }],
            ],
          },
          'VCLibrarianTool': {
            'AdditionalOptions': ['/ignore:4221'],
          },
          'VCLinkerTool': {
            'AdditionalDependencies': [
            ],
            'GenerateDebugInformation': 'true',
          },
        },
      },
    }],  # OS=="win"
    ['OS=="mac"', {
      'target_defaults': {
        'xcode_settings': {
          'ALWAYS_SEARCH_USER_PATHS': 'NO',
          'GCC_C_LANGUAGE_STANDARD': 'ansi',        # -ansi
          'GCC_CW_ASM_SYNTAX': 'NO',                # No -fasm-blocks
          'GCC_DYNAMIC_NO_PIC': 'NO',               # No -mdynamic-no-pic
                                                    # (Equivalent to -fPIC)
          'GCC_ENABLE_CPP_EXCEPTIONS': 'NO',        # -fno-exceptions
          'GCC_ENABLE_CPP_RTTI': 'NO',              # -fno-rtti
          'GCC_ENABLE_PASCAL_STRINGS': 'NO',        # No -mpascal-strings
          # GCC_INLINES_ARE_PRIVATE_EXTERN maps to -fvisibility-inlines-hidden
          'GCC_INLINES_ARE_PRIVATE_EXTERN': 'YES',
          'GCC_SYMBOLS_PRIVATE_EXTERN': 'YES',      # -fvisibility=hidden
          'GCC_THREADSAFE_STATICS': 'NO',           # -fno-threadsafe-statics
          'GCC_TREAT_WARNINGS_AS_ERRORS': 'YES',    # -Werror
          'GCC_VERSION': '4.2',
          'GCC_WARN_ABOUT_MISSING_NEWLINE': 'YES',  # -Wnewline-eof
          'MACOSX_DEPLOYMENT_TARGET': '10.4',       # -mmacosx-version-min=10.4
          'PREBINDING': 'NO',                       # No -Wl,-prebind
          'USE_HEADERMAP': 'NO',
          'OTHER_CFLAGS': [
            '-fno-strict-aliasing',
          ],
          'WARNING_CFLAGS': [
            '-Wall',
            '-Wendif-labels',
            '-W',
            '-Wno-unused-parameter',
            '-Wnon-virtual-dtor',
          ],
        },
        'target_conditions': [
          ['_type!="static_library"', {
            'xcode_settings': {'OTHER_LDFLAGS': ['-Wl,-search_paths_first']},
          }],
        ],  # target_conditions
      },  # target_defaults
    }],  # OS=="mac"
  ],
}
