{
  'targets': [
    {
      'target_name': 'uvwasi',
      'type': 'static_library',
      'cflags': ['-fvisibility=hidden'],
      'xcode_settings': {
        'GCC_SYMBOLS_PRIVATE_EXTERN': 'YES',  # -fvisibility=hidden
      },
      'include_dirs': ['include'],
      'sources': [
        'src/clocks.c',
        'src/fd_table.c',
        'src/path_resolver.c',
        'src/poll_oneoff.c',
        'src/uv_mapping.c',
        'src/uvwasi.c',
        'src/wasi_rights.c',
        'src/wasi_serdes.c',
      ],
      'direct_dependent_settings': {
        'include_dirs': ['include']
      },
      'conditions': [
        [ 'OS=="linux"', {
          'defines': [
            '_GNU_SOURCE',
            '_POSIX_C_SOURCE=200112',
          ],
        }],
        [ 'node_shared_libuv=="false"', {
          'dependencies': [
            '../uv/uv.gyp:libuv',
          ],
        }],
      ],
    }
  ]
}
