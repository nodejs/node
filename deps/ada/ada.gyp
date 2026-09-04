{
  'variables': {
    'ada_sources': [ 'ada.cpp' ],
  },
  'targets': [
    {
      'target_name': 'ada',
      'type': 'static_library',
      'include_dirs': [
        '.',
        '<(DEPTH)/deps/v8/third_party/simdutf',
      ],
      'direct_dependent_settings': {
        'include_dirs': ['.'],
      },
      'defines': [
        'ADA_USE_SIMDUTF=1',
      ],
      'dependencies': [
        '../../tools/v8_gypfiles/v8.gyp:simdutf',
      ],
      'sources': [ '<@(ada_sources)' ],
      'conditions': [
        # See the same setting in node.gyp.
        ['node_shared=="false" and OS=="mac"', {
          'xcode_settings': {
            'GCC_SYMBOLS_PRIVATE_EXTERN': 'YES',  # -fvisibility=hidden
            'GCC_INLINES_ARE_PRIVATE_EXTERN': 'YES'  # -fvisibility-inlines-hidden
          },
        }, 'node_shared=="false" and (OS!="aix" and OS!="os400") and (OS!="win" or clang==1)', {
          'cflags': [
            '-fvisibility=hidden',
            '-fvisibility-inlines-hidden'
          ],
        }],  # MSVC hides the non-public symbols by default so no need to configure it.
      ],
    },
  ]
}
