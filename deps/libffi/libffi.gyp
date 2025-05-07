{
  'targets': [{
    'target_name': 'libffi',
    'type': 'static_library',
    'include_dirs': ['fixed', 'include'],
    'direct_dependent_settings': {
      'include_dirs': ['fixed', 'include'],
    },
    'sources': [
      'fixed/ffi.c',
      'fixed/ffiasm.S',
      'src/closures.c',
      'src/debug.c',
      # dlmalloc.c has already been included in closures.c
      'src/java_raw_api.c',
      'src/prep_cif.c',
      'src/raw_api.c',
      'src/tramp.c',
      'src/types.c',
    ],
    'conditions': [
      ['GENERATOR=="msvs"', {
        'actions': [{
          'action_name': 'generate_msvc_asm',
          'message': 'Generating Microsoft ASM file from ffiasm.S',
          'inputs': ['fixed/ffiasm.S'],
          'outputs': ['<(INTERMEDIATE_DIR)/ffiasm.asm'],
          'action': [
            'cl',
            '-nologo',
            '-EP',
            '-P',
            '-I',
            './fixed',
            '-I',
            './include',
            '-Fi<(INTERMEDIATE_DIR)/ffiasm.asm',
            './fixed/ffiasm.S',
          ],
          'msvs_cygwin_shell': 0,
          'process_outputs_as_sources': 1,
        }],
        'msvs_settings': {
          'MASM': {
            'UseSafeExceptionHandlers': 'true',
          },
        },
      }]
    ]
  }]
}
