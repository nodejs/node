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
            'x86.h',
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
              'defines': [ 'HAVE_HIDDEN' ],
            }],
            ['OS=="mac" or OS=="ios" or OS=="freebsd" or OS=="android"', {
              # Mac, Android and the BSDs don't have fopen64, ftello64, or
              # fseeko64. We use fopen, ftell, and fseek instead on these
              # systems.
              'defines': [
                'USE_FILE32API'
              ],
            }],
            ['(target_arch in "ia32 x64 x32" and OS!="ios") or arm_fpu=="neon"', {
              'sources': [
                'adler32_simd.c',
                'adler32_simd.h',
                'contrib/optimizations/chunkcopy.h',
                'contrib/optimizations/inffast_chunk.c',
                'contrib/optimizations/inffast_chunk.h',
                'contrib/optimizations/inflate.c',
              ],
            }, {
              'sources': [ 'inflate.c', ],
            }],
            # Incorporate optimizations where possible
            ['target_arch in "ia32 x64 x32" and OS!="ios"', {
              'defines': [
                'ADLER32_SIMD_SSSE3',
                'INFLATE_CHUNK_SIMD_SSE2',
                'CRC32_SIMD_SSE42_PCLMUL',
              ],
              'sources': [
                'crc32_simd.c',
                'crc32_simd.h',
                'crc_folding.c',
                'fill_window_sse.c',
                'x86.c',
              ],
              'conditions': [
                ['target_arch=="x64"', {
                  'defines': [ 'INFLATE_CHUNK_READ_64LE' ],
                }],
              ],
            }, {
              'sources': [ 'simd_stub.c', ],
            }],
            ['arm_fpu=="neon"', {
              'defines': [
                'ADLER32_SIMD_NEON',
                'INFLATE_CHUNK_SIMD_NEON',
              ],
              'sources': [
                'contrib/optimizations/slide_hash_neon.h',
              ],
              'conditions': [
                ['OS!="ios"', {
                  'defines': [ 'CRC32_ARMV8_CRC32' ],
                  'sources': [
                    'arm_features.c',
                    'arm_features.h',
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
                    ['OS="win"', {
                      'defines': [ 'ARMV8_OS_WINDOWS' ],
                    }],
                    ['OS!="android" and OS!="win" and llvm_version=="0.0"', {
                      'cflags': [
                        '-march=armv8-a+crc',
                      ],
                    }],
                  ],
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
