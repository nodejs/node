# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'use_system_zlib%': 0,
    'arm_fpu%': '',
    'llvm_version%': '0.0',
  },
  'conditions': [
    ['use_system_zlib==0', {
      'targets': [
        {
          'target_name': 'zlib',
          'type': 'static_library',
          'sources': [
            'adler32.c',
            'compress.c',
            'contrib/optimizations/insert_string.h',
            'cpu_features.c',
            'cpu_features.h',
            'crc32.c',
            'crc32.h',
            'deflate.c',
            'deflate.h',
            'gzclose.c',
            'gzguts.h',
            'gzlib.c',
            'gzread.c',
            'gzwrite.c',
            'infback.c',
            'inffast.c',
            'inffast.h',
            'inffixed.h',
            'inflate.h',
            'inftrees.c',
            'inftrees.h',
            'trees.c',
            'trees.h',
            'uncompr.c',
            'zconf.h',
            'zlib.h',
            'zutil.c',
            'zutil.h',
          ],
          'include_dirs': [
            '.',
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              '.',
            ],
          },
          'conditions': [
            ['OS!="win"', {
              'cflags!': [ '-ansi' ],
              'cflags': [ '-Wno-implicit-fallthrough' ],
              'defines': [ 'HAVE_HIDDEN' ],
            }],
            ['OS=="mac" or OS=="freebsd" or OS=="android"', {
              # Mac, Android and the BSDs don't have fopen64, ftello64, or
              # fseeko64. We use fopen, ftell, and fseek instead on these
              # systems.
              'defines': [
                'USE_FILE32API'
              ],
            }],
            ['(target_arch in "ia32 x64" and OS!="ios") or arm_fpu=="neon"', {
              'sources': [
                'contrib/optimizations/chunkcopy.h',
                'contrib/optimizations/inffast_chunk.c',
                'contrib/optimizations/inffast_chunk.h',
                'contrib/optimizations/inflate.c',
                'slide_hash_simd.h'
              ],
            }, {
              'sources': [ 'inflate.c', ],
            }],
            # Incorporate optimizations where possible
            ['target_arch in "ia32 x64" and OS!="ios"', {
              'defines': [
                'ADLER32_SIMD_SSSE3',
                'INFLATE_CHUNK_SIMD_SSE2',
                'CRC32_SIMD_SSE42_PCLMUL',
                'DEFLATE_SLIDE_HASH_SSE2'
              ],
              'sources': [
                'adler32_simd.c',
                'adler32_simd.h',
                'crc32_simd.c',
                'crc32_simd.h',
                'crc_folding.c'
              ],
              'conditions': [
                ['target_arch=="x64"', {
                  'defines': [ 'INFLATE_CHUNK_READ_64LE' ],
                }],
                ['OS=="win"', {
                  'defines': [ 'X86_WINDOWS' ]
                }, {
                  'defines': [ 'X86_NOT_WINDOWS' ]
                }]
              ],
            }],
            ['arm_fpu=="neon"', {
              'defines': [ '__ARM_NEON__' ],
              'conditions': [
                ['OS=="win"', {
                  'defines': [
                    'ARMV8_OS_WINDOWS',
                    'DEFLATE_SLIDE_HASH_NEON',
                    'INFLATE_CHUNK_SIMD_NEON'
                  ]
                }, {
                  'conditions': [
                    ['OS!="ios"', {
                      'defines': [
                        'ADLER32_SIMD_NEON',
                        'CRC32_ARMV8_CRC32',
                        'DEFLATE_SLIDE_HASH_NEON',
                        'INFLATE_CHUNK_SIMD_NEON'
                      ],
                      'sources': [
                        'adler32_simd.c',
                        'adler32_simd.h',
                        'crc32_simd.c',
                        'crc32_simd.h',
                      ],
                      'conditions': [
                        ['OS=="android"', {
                          'defines': [ 'ARMV8_OS_ANDROID' ],
                        }],
                        ['OS=="linux"', {
                          'defines': [ 'ARMV8_OS_LINUX' ],
                        }],
                        ['OS=="mac"', {
                          'defines': [ 'ARMV8_OS_MACOS' ],
                        }],
                        ['llvm_version=="0.0"', {
                          'cflags': [
                            '-march=armv8-a+aes+crc',
                          ],
                        }],
                      ],
                    }]
                  ]
                }],
                ['target_arch=="arm64"', {
                  'defines': [ 'INFLATE_CHUNK_READ_64LE' ],
                }],
              ],
            }],
          ],
        },
      ],
    }, {
      'targets': [
        {
          'target_name': 'zlib',
          'type': 'static_library',
          'direct_dependent_settings': {
            'defines': [
              'USE_SYSTEM_ZLIB',
            ],
          },
          'defines': [
            'USE_SYSTEM_ZLIB',
          ],
          'link_settings': {
            'libraries': [
              '-lz',
            ],
          },
        },
      ],
    }],
  ],
}
