{
  'variables': {
    'zstd_sources': [
      # cxx_library(name='debug')
      'lib/common/debug.c',

      # cxx_library(name='bitstream')
      # [no .c files]

      # cxx_library(name='cpu')
      # [no .c files]

      # cxx_library(name='entropy')
      'lib/common/entropy_common.c',
      'lib/common/fse_decompress.c',
      'lib/compress/fse_compress.c',
      'lib/compress/huf_compress.c',
      'lib/decompress/huf_decompress.c',

      # cxx_library(name='pool')
      'lib/common/pool.c',

      # cxx_library(name='threading')
      'lib/common/threading.c',

      # cxx_library(name='xxhash')
      'lib/common/xxhash.c',

      # cxx_library(name='zstd_common')
      'lib/common/zstd_common.c',

      # cxx_library(name='errors')
      'lib/common/error_private.c',

      # cxx_library(name='mem')
      # [no .c files]

      # cxx_library(name='compiler')
      # [no .c files]

      # cxx_library(name='compress')
      'lib/compress/hist.c',
      # glob(compress/zstd*.c)
      'lib/compress/zstd_compress.c',
      'lib/compress/zstd_compress_literals.c',
      'lib/compress/zstd_compress_sequences.c',
      'lib/compress/zstd_compress_superblock.c',
      'lib/compress/zstd_double_fast.c',
      'lib/compress/zstd_fast.c',
      'lib/compress/zstd_lazy.c',
      'lib/compress/zstd_ldm.c',
      'lib/compress/zstd_opt.c',
      'lib/compress/zstd_preSplit.c',
      'lib/compress/zstdmt_compress.c',

      # cxx_library(name='decompress')
      # glob(decompress/zstd*.c)
      'lib/decompress/zstd_ddict.c',
      'lib/decompress/zstd_decompress.c',
      'lib/decompress/zstd_decompress_block.c',
    ],
  },
  'targets': [
    {
      'target_name': 'zstd',
      'type': 'static_library',
      'include_dirs': ['lib'],
      # -pthread?
      'direct_dependent_settings': {
        'include_dirs': [ 'lib' ]
      },
      'defines': [
        # cxx_library(name='xxhash')
        'XXH_NAMESPACE=ZSTD_',
        # cxx_library(name='threading')
        'ZSTD_MULTITHREAD',
        # TODO: Use deps/zstd/lib/decompress/huf_decompress_amd64.S.
        'ZSTD_DISABLE_ASM',
      ],
      'all_dependent_settings': {
        'defines': [
          'XXH_NAMESPACE=ZSTD_',
          'ZSTD_MULTITHREAD',
          # TODO: Use deps/zstd/lib/decompress/huf_decompress_amd64.S.
          'ZSTD_DISABLE_ASM',
        ],
      },
      'conditions': [
        [ 'OS=="solaris"', {
          'cflags': [ '-pthreads' ],
          'ldflags': [ '-pthreads' ],
        }],
        [ 'OS in "freebsd dragonflybsd linux openbsd aix os400"', {
          'cflags': [ '-pthread' ],
          'ldflags': [ '-pthread' ],
        }],
      ],
      'libraries': [
        '-lzstd',
      ],
      'sources': [
        '<@(zstd_sources)',
      ]
    }
  ]
}
