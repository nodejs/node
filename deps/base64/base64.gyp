{
  'variables': {
    'arm_fpu%': '',
    'target_arch%': '',
    'base64_sources_common': [
      'base64/include/libbase64.h',
      'base64/lib/arch/generic/codec.c',
      'base64/lib/tables/tables.c',
      'base64/lib/codec_choose.c',
      'base64/lib/codecs.h',
      'base64/lib/lib.c',
    ],
  },
  'targets': [
    {
      'target_name': 'base64',
      'type': 'static_library',
      'include_dirs': [ 'base64/include', 'base64/lib' ],
      'direct_dependent_settings': {
        'include_dirs': [ 'base64/include' ],
        'defines': [ 'BASE64_STATIC_DEFINE' ],
      },
      'defines': [ 'BASE64_STATIC_DEFINE' ],
      'sources': [
        '<@(base64_sources_common)',
      ],

      'conditions': [
        [ 'arm_fpu=="neon" and target_arch=="arm"', {
          'defines': [ 'HAVE_NEON32=1' ],
          'dependencies': [ 'base64_neon32' ],
        }, {
          'sources': [ 'base64/lib/arch/neon32/codec.c' ],
        }],

        # arm64 requires NEON, so it's safe to always use it
        [ 'target_arch=="arm64"', {
          'defines': [ 'HAVE_NEON64=1' ],
          'dependencies': [ 'base64_neon64' ],
        }, {
          'sources': [ 'base64/lib/arch/neon64/codec.c' ],
        }],

        # Runtime detection will happen for x86 CPUs
        [ 'target_arch in "ia32 x64 x32"', {
          'defines': [
            'HAVE_SSSE3=1',
            'HAVE_SSE41=1',
            'HAVE_SSE42=1',
            'HAVE_AVX=1',
            'HAVE_AVX2=1',
          ],
          'dependencies': [
            'base64_ssse3',
            'base64_sse41',
            'base64_sse42',
            'base64_avx',
            'base64_avx2',
          ],
        }, {
          'sources': [
            'base64/lib/arch/ssse3/codec.c',
            'base64/lib/arch/sse41/codec.c',
            'base64/lib/arch/sse42/codec.c',
            'base64/lib/arch/avx/codec.c',
            'base64/lib/arch/avx2/codec.c',
          ],
        }],
      ],
    },

    {
      'target_name': 'base64_ssse3',
      'type': 'static_library',
      'include_dirs': [ 'base64/include', 'base64/lib' ],
      'sources': [ 'base64/lib/arch/ssse3/codec.c' ],
      'defines': [ 'BASE64_STATIC_DEFINE', 'HAVE_SSSE3=1' ],
      'conditions': [
        [ 'OS!="win"', {
          'cflags': [ '-mssse3' ],
          'xcode_settings': {
            'OTHER_CFLAGS': [ '-mssse3' ]
          },
        }],
      ],
    },

    {
      'target_name': 'base64_sse41',
      'type': 'static_library',
      'include_dirs': [ 'base64/include', 'base64/lib' ],
      'sources': [ 'base64/lib/arch/sse41/codec.c' ],
      'defines': [ 'BASE64_STATIC_DEFINE', 'HAVE_SSE41=1' ],
      'conditions': [
        [ 'OS!="win"', {
          'cflags': [ '-msse4.1' ],
          'xcode_settings': {
            'OTHER_CFLAGS': [ '-msse4.1' ]
          },
        }],
      ],
    },

    {
      'target_name': 'base64_sse42',
      'type': 'static_library',
      'include_dirs': [ 'base64/include', 'base64/lib' ],
      'sources': [ 'base64/lib/arch/sse42/codec.c' ],
      'defines': [ 'BASE64_STATIC_DEFINE', 'HAVE_SSE42=1' ],
      'conditions': [
        [ 'OS!="win"', {
          'cflags': [ '-msse4.2' ],
          'xcode_settings': {
            'OTHER_CFLAGS': [ '-msse4.2' ]
          },
        }],
      ],
    },

    {
      'target_name': 'base64_avx',
      'type': 'static_library',
      'include_dirs': [ 'base64/include', 'base64/lib' ],
      'sources': [ 'base64/lib/arch/avx/codec.c' ],
      'defines': [ 'BASE64_STATIC_DEFINE', 'HAVE_AVX=1' ],
      'conditions': [
        [ 'OS!="win"', {
          'cflags': [ '-mavx' ],
          'xcode_settings': {
            'OTHER_CFLAGS': [ '-mavx' ]
          },
        }, {
          'msvs_settings': {
            'VCCLCompilerTool': {
              'AdditionalOptions': [
                '/arch:AVX'
              ],
            },
          },
        }],
      ],
    },

    {
      'target_name': 'base64_avx2',
      'type': 'static_library',
      'include_dirs': [ 'base64/include', 'base64/lib' ],
      'sources': [ 'base64/lib/arch/avx2/codec.c' ],
      'defines': [ 'BASE64_STATIC_DEFINE', 'HAVE_AVX2=1' ],
      'conditions': [
        [ 'OS!="win"', {
          'cflags': [ '-mavx2' ],
          'xcode_settings': {
            'OTHER_CFLAGS': [ '-mavx2' ]
          },
        }, {
          'msvs_settings': {
            'VCCLCompilerTool': {
              'AdditionalOptions': [
                '/arch:AVX2'
              ],
            },
          },
        }],
      ],
    },

    {
      'target_name': 'base64_neon32',
      'type': 'static_library',
      'include_dirs': [ 'base64/include', 'base64/lib' ],
      'sources': [ 'base64/lib/arch/neon32/codec.c' ],
      'defines': [ 'BASE64_STATIC_DEFINE', 'HAVE_NEON32=1' ],
      'conditions': [
        [ 'OS!="win"', {
          'cflags': [ '-mfpu=neon' ],
          'xcode_settings': {
            'OTHER_CFLAGS': [ '-mfpu=neon' ]
          },
        }],
      ],
    },

    {
      'target_name': 'base64_neon64',
      'type': 'static_library',
      'include_dirs': [ 'base64/include', 'base64/lib' ],
      'sources': [ 'base64/lib/arch/neon64/codec.c' ],
      'defines': [ 'BASE64_STATIC_DEFINE', 'HAVE_NEON64=1' ],
      # NEON is required in arm64, so no -mfpu flag is needed
    }

  ]
}
