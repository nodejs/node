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
              'src/x86/win64_intel.S',
            ],
            'libffi_defines': [
              'LIBFFI_HIDE_BASIC_TYPES',
            ],
          },
        }],
        ['target_arch == "arm"', {
          'variables': {
            'libffi_arch_sources': [
              'src/arm/ffi.c',
              'src/arm/sysv_msvc_arm32.S',
            ],
            'libffi_defines': [
              'LIBFFI_HIDE_BASIC_TYPES',
            ],
          },
        }],
        ['target_arch == "arm64"', {
          'variables': {
            'libffi_arch_sources': [
              'src/aarch64/ffi.c',
              'src/aarch64/win64_armasm.S',
            ],
            'libffi_defines': [
              'LIBFFI_HIDE_BASIC_TYPES',
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
              'src/x86/ffi64.c',
              'src/x86/unix64.S',
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
            '--output-dir=<(INTERMEDIATE_DIR)',
            '--target-arch=<(target_arch)',
            '--os=<(OS)',
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
