{
  'variables': {
    'libffi_sources': [
      'src/closures.c',
      'src/debug.c',
      'src/java_raw_api.c',
      'src/prep_cif.c',
      'src/raw_api.c',
      'src/tramp.c',
      'src/types.c',
    ],
    'libffi_defines%': [],
    'libffi_arch_sources%': [],
  },
  'conditions': [
    ['OS == "win"', {
      'conditions': [
        ['target_arch == "x64"', {
          'variables': {
            'libffi_arch_sources': [
              'src/x86/ffiw64.c',
            ],
          },
        }],
        ['target_arch == "arm"', {
          'variables': {
            'libffi_arch_sources': [
              'src/arm/ffi.c',
              'src/arm/sysv_msvc_arm32.S',
            ],
          },
        }],
        ['target_arch == "arm64"', {
          'variables': {
            'libffi_arch_sources': [
              'src/aarch64/ffi.c',
            ],
          },
        }],
      ],
    }],
    ['OS == "linux" or OS == "freebsd"', {
      'conditions': [
        ['target_arch == "x64"', {
          'variables': {
            'libffi_arch_sources': [
              'src/x86/ffiw64.c',
              'src/x86/ffi64.c',
              'src/x86/unix64.S',
              'src/x86/win64.S',
            ],
          },
        }],
        ['target_arch == "arm"', {
          'variables': {
            'libffi_arch_sources': [
              'src/arm/ffi.c',
              'src/arm/sysv.S',
            ],
          },
        }],
        ['target_arch == "arm64"', {
          'variables': {
            'libffi_arch_sources': [
              'src/aarch64/ffi.c',
              'src/aarch64/sysv.S',
            ],
          },
        }],
      ],
    }],
    ['OS == "mac"', {
      'conditions': [
        ['target_arch == "x64"', {
          'variables': {
            'libffi_arch_sources': [
              'src/x86/ffiw64.c',
              'src/x86/ffi64.c',
              'src/x86/unix64.S',
              'src/x86/win64.S',
            ],
          },
        }],
        ['target_arch == "arm64"', {
          'variables': {
            'libffi_arch_sources': [
              'src/aarch64/ffi.c',
              'src/aarch64/sysv.S',
            ],
          },
        }],
      ],
    }],
  ],
  'targets': [
    {
      'target_name': 'libffi',
      'type': 'static_library',
      'cflags': ['-fvisibility=hidden'],
      'xcode_settings': {
        'GCC_SYMBOLS_PRIVATE_EXTERN': 'YES',
      },
      'defines': [
        'FFI_STATIC_BUILD',
        '<@(libffi_defines)',
      ],
      'include_dirs': [
        'include',
        '<(INTERMEDIATE_DIR)',
      ],
      'sources': [
        '<@(libffi_sources)',
        '<@(libffi_arch_sources)',
      ],
      'actions': [
        {
          'action_name': 'generate_libffi_headers',
          'inputs': [
            'generate-headers.py',
            'include/ffi.h.in',
            'src/aarch64/ffitarget.h',
            'src/arm/ffitarget.h',
            'src/x86/ffitarget.h',
          ],
          'outputs': [
            '<(INTERMEDIATE_DIR)/ffi.h',
            '<(INTERMEDIATE_DIR)/fficonfig.h',
            '<(INTERMEDIATE_DIR)/ffitarget.h',
          ],
          'action': [
            '<(python)',
            'generate-headers.py',
            '--output-dir',
            '<(INTERMEDIATE_DIR)',
          ],
        },
      ],
      'conditions': [
        ['OS == "win" and target_arch == "x64"', {
          'actions': [
            {
              'action_name': 'preprocess_win64_intel_asm',
              'process_outputs_as_sources': 1,
              'inputs': [
                'preprocess_asm.py',
                'include/ffi_cfi.h',
                'src/x86/asmnames.h',
                'src/x86/win64_intel.S',
                '<(INTERMEDIATE_DIR)/ffi.h',
                '<(INTERMEDIATE_DIR)/fficonfig.h',
              ],
              'outputs': [
                '<(INTERMEDIATE_DIR)/win64_intel.asm',
              ],
              'action': [
                '<(python)',
                'preprocess_asm.py',
                '--input',
                'src/x86/win64_intel.S',
                '--output',
                '<@(_outputs)',
                '--include-dir',
                'include',
                '--include-dir',
                'src/x86',
                '--define',
                'FFI_STATIC_BUILD',
              ],
            },
          ],
        }],
        ['OS == "win" and target_arch == "arm64"', {
          # Link the prebuilt object file directly
          'libraries': [
            '<(INTERMEDIATE_DIR)/win64_armasm.obj',
          ],
          'actions': [
            {
              'action_name': 'assemble_win64_arm_asm',
              # Don't use process_outputs_as_sources - we link the .obj directly
              'inputs': [
                'preprocess_asm.py',
                'include/ffi_cfi.h',
                'src/aarch64/internal.h',
                'src/aarch64/win64_armasm.S',
                '<(INTERMEDIATE_DIR)/ffi.h',
                '<(INTERMEDIATE_DIR)/fficonfig.h',
              ],
              'outputs': [
                '<(INTERMEDIATE_DIR)/win64_armasm.obj',
              ],
              'action': [
                '<(python)',
                'preprocess_asm.py',
                '--input',
                'src/aarch64/win64_armasm.S',
                '--output',
                '<(INTERMEDIATE_DIR)/win64_armasm.asm',
                '--include-dir',
                'include',
                '--include-dir',
                'src/aarch64',
                '--define',
                'FFI_STATIC_BUILD',
                '--assemble',
                '<@(_outputs)',
              ],
            },
          ],
        }],
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'include',
          '<(INTERMEDIATE_DIR)',
        ],
        'defines': [
          'FFI_STATIC_BUILD',
        ],
      },
    },
  ],
}
