# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'use_system_zlib%': 0
  },
  'conditions': [
    ['use_system_zlib==0', {
      'targets': [
        {
          'target_name': 'zlib',
          'type': 'static_library',
          'sources': [
            'contrib/minizip/ioapi.c',
            'contrib/minizip/ioapi.h',
            'contrib/minizip/iowin32.c',
            'contrib/minizip/iowin32.h',
            'contrib/minizip/unzip.c',
            'contrib/minizip/unzip.h',
            'contrib/minizip/zip.c',
            'contrib/minizip/zip.h',
            'adler32.c',
            'compress.c',
            'crc32.c',
            'crc32.h',
            'deflate.c',
            'deflate.h',
            'gzio.c',
            'infback.c',
            'inffast.c',
            'inffast.h',
            'inffixed.h',
            'inflate.c',
            'inflate.h',
            'inftrees.c',
            'inftrees.h',
            'mozzconf.h',
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
            # For contrib/minizip
            './contrib/minizip',
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              '.',
            ],
          },
          'conditions': [
            ['OS!="win"', {
              'product_name': 'chrome_zlib',
              'cflags!': [ '-ansi' ],
              'sources!': [
                'contrib/minizip/iowin32.c'
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
          'sources': [
            'contrib/minizip/ioapi.c',
            'contrib/minizip/ioapi.h',
            'contrib/minizip/unzip.c',
            'contrib/minizip/unzip.h',
            'contrib/minizip/zip.c',
            'contrib/minizip/zip.h',
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
