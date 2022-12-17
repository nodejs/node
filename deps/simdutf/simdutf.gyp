{
  'variables': {
    'gas_version%': '0.0',
    'nasm_version%': '0.0',
    'llvm_version%': '0.0',
  },
  'targets': [
    {
      'target_name': 'simdutf',
      'type': 'static_library',
      'include_dirs': ['.'],
      'direct_dependent_settings': {
        'include_dirs': ['.'],
      },
      'sources': ['simdutf.cpp'],
      'conditions': [
        ['OS=="linux"', {
          'conditions': [
            # TODO(anonrig): Remove this validation when benchmark CI has binutils >= 2.30
            ['v(gas_version) < v("2.30") and v(nasm_version) < v("2.14") and v(llvm_version) < v("6.0")', {
              'defines': ['SIMDUTF_IMPLEMENTATION_ICELAKE=0'],
            }],
          ],
        }],
      ],
    },
  ]
}
