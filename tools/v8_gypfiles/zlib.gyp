# Minimal port of deps/v8/third_party/zlib/BUILD.gn
{
  'variables': {
    'ZLIB_ROOT': '../../deps/v8/third_party/zlib',
  },
  'targets': [
    {
      'target_name': 'zlib',
      'type': 'static_library',
      'toolsets': ['target'],
      'dependencies': [
        'zlib_x86_simd',
      ],
      'defines': ['ZLIB_IMPLEMENTATION'],
      'sources': [
        '<(ZLIB_ROOT)/adler32.c',
        '<(ZLIB_ROOT)/chromeconf.h',
        '<(ZLIB_ROOT)/compress.c',
        '<(ZLIB_ROOT)/crc32.c',
        '<(ZLIB_ROOT)/crc32.h',
        '<(ZLIB_ROOT)/deflate.c',
        '<(ZLIB_ROOT)/deflate.h',
        '<(ZLIB_ROOT)/gzclose.c',
        '<(ZLIB_ROOT)/gzguts.h',
        '<(ZLIB_ROOT)/gzlib.c',
        '<(ZLIB_ROOT)/gzread.c',
        '<(ZLIB_ROOT)/gzwrite.c',
        '<(ZLIB_ROOT)/infback.c',
        '<(ZLIB_ROOT)/inffast.c',
        '<(ZLIB_ROOT)/inffast.h',
        '<(ZLIB_ROOT)/inffixed.h',
        '<(ZLIB_ROOT)/inflate.h',
        '<(ZLIB_ROOT)/inftrees.c',
        '<(ZLIB_ROOT)/inftrees.h',
        '<(ZLIB_ROOT)/trees.c',
        '<(ZLIB_ROOT)/trees.h',
        '<(ZLIB_ROOT)/uncompr.c',
        '<(ZLIB_ROOT)/x86.h',
        '<(ZLIB_ROOT)/zconf.h',
        '<(ZLIB_ROOT)/zlib.h',
        '<(ZLIB_ROOT)/zutil.c',
        '<(ZLIB_ROOT)/zutil.h',
      ],
      'conditions': [
        # TODO: correctly implement the conditions
        ['1 == 0', {
          'dependencies': [
            'zlib_adler32_simd',
            'zlib_inflate_chunk_simd',
          ],
          'conditions': [
            ['1 == 0', {
              'sources': [
                '<(ZLIB_ROOT)/x86.c',
              ],
              'dependencies': ['zlib_crc32_simd'],
            }],
            ['1 == 0', {
              'sources': [
                '<(ZLIB_ROOT)/contrib/optimizations/slide_hash_neon.h',
              ],
              'dependencies': ['zlib_arm_crc32'],
            }],
          ],
        }, {
          'sources': [
            '<(ZLIB_ROOT)/inflate.c',
          ],
        }],
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(ZLIB_ROOT)',
        ],
      },
    },  # zlib
    {
      'target_name': 'zlib_x86_simd',
      'type': 'none',
      'hard_dependency': 1,
      'defines': ['ZLIB_IMPLEMENTATION'],
      'direct_dependent_settings': {
        # TODO: implement the conditions
        'sources': [
          '<(ZLIB_ROOT)/simd_stub.c',
        ],
      },
    },  # zlib_x86_simd
  ],
}
