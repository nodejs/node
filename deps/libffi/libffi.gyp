{
  'variables': {
    'libffi_sources': [
      'src/closures.c',
      'src/debug.c',
      'src/java_raw_api.c',
      'src/prep_cif.c',
      'src/raw_api.c',
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
              'src/x86/win64.S',
            ],
            'libffi_defines': [
              'LIBFFI_HIDE_BASIC_TYPES',
            ],
          },
        }],
        ['target_arch == "arm"', {
          'variables': {
            'libffi_arch_sources': [
              'src/arm/sysv.c',
              'src/arm/armeabi.c',
              'src/arm/win32.c',
            ],
            'libffi_defines': [
              'LIBFFI_HIDE_BASIC_TYPES',
            ],
          },
        }],
        ['target_arch == "arm64"', {
          'variables': {
            'libffi_arch_sources': [
              'src/aarch64/sysv.c',
              'src/aarch64/win64_armasm.c',
            ],
            'libffi_defines': [
              'LIBFFI_HIDE_BASIC_TYPES',
            ],
          },
        }],
        ['target_arch == "x86"', {
          # Windows x86 is not supported by libffi
          'variables': {
            'libffi_arch_sources': [],
          },
        }],
      ],
    }],
    ['OS == "linux" or OS == "freebsd"', {
      'conditions': [
        ['target_arch == "x64"', {
          'variables': {
            'libffi_arch_sources': [
              'src/x86/ffi64.c',
              'src/x86/unix64.S',
            ],
          },
        }],
        ['target_arch == "x86"', {
          'variables': {
            'libffi_arch_sources': [
              'src/x86/ffi.c',
              'src/x86/sysv.S',
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
              'src/x86/ffi64.c',
              'src/x86/unix64.S',
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
        'FFI_BUILDING',
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
          ],
          'outputs': [
            '<(INTERMEDIATE_DIR)/ffi.h',
            '<(INTERMEDIATE_DIR)/fficonfig.h',
            '<(INTERMEDIATE_DIR)/ffitarget.h',
          ],
          'action': [
            '<(python)',
            'generate-headers.py',
            '<(INTERMEDIATE_DIR)',
          ],
        },
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'include',
          '<(INTERMEDIATE_DIR)',
        ],
        'defines': [
          'FFI_BUILDING',
        ],
      },
    },
  ],
}
