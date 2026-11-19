{
  'variables': {
    'jsonifier_include_dir%': 'include',
  },
  'targets': [
    {
      'target_name': 'jsonifier',
      'includes': [ 'jsonifier_target.gypi' ],
      'sources': [
        'package_json_parser.cc',
      ],
      'conditions': [
        ['target_arch=="x64"', {
          'dependencies': [ 'jsonifier_avx2', 'jsonifier_avx512' ],
          'sources': [
            'package_json_parser_fallback.cc',
          ],
        }],
        ['target_arch=="arm64"', {
          'sources': [
            'package_json_parser_neon.cc',
          ],
        }],
        ['target_arch!="x64" and target_arch!="arm64"', {
          'sources': [
            'package_json_parser_fallback.cc',
          ],
        }],
      ],
    },
  ],
  'conditions': [
    ['target_arch=="x64"', {
      'targets': [
        {
          'target_name': 'jsonifier_avx2',
          'includes': [ 'jsonifier_target.gypi' ],
          'sources': [
            'package_json_parser_avx2.cc',
          ],
          'cflags_cc': [ '-mavx2', '-mbmi', '-mbmi2', '-mlzcnt', '-mpopcnt', '-mpclmul' ],
          'xcode_settings': {
            'OTHER_CPLUSPLUSFLAGS': [ '-mavx2', '-mbmi', '-mbmi2', '-mlzcnt', '-mpopcnt', '-mpclmul' ],
          },
          'msvs_settings': {
            'VCCLCompilerTool': {
              'EnableEnhancedInstructionSet': '5',
            },
          },
        },
        {
          'target_name': 'jsonifier_avx512',
          'includes': [ 'jsonifier_target.gypi' ],
          'sources': [
            'package_json_parser_avx512.cc',
          ],
          'cflags_cc': [ '-mavx2', '-mbmi', '-mbmi2', '-mlzcnt', '-mpopcnt', '-mpclmul', '-mavx512f', '-mavx512dq', '-mavx512bw', '-mavx512vl', '-mavx512vbmi2' ],
          'xcode_settings': {
            'OTHER_CPLUSPLUSFLAGS': [ '-mavx2', '-mbmi', '-mbmi2', '-mlzcnt', '-mpopcnt', '-mpclmul', '-mavx512f', '-mavx512dq', '-mavx512bw', '-mavx512vl', '-mavx512vbmi2' ],
          },
          'msvs_settings': {
            'VCCLCompilerTool': {
              'AdditionalOptions': [ '-mavx2', '-mbmi', '-mbmi2', '-mlzcnt', '-mpopcnt', '-mpclmul', '-mavx512f', '-mavx512dq', '-mavx512bw', '-mavx512vl', '-mavx512vbmi2' ],
            },
          },
        },
      ],
    }],
  ],
}
