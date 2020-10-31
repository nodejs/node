{
  'targets': [
    {
      'target_name': 'jsoncpp',
      'type': 'static_library',
      'cflags': ['-fvisibility=hidden'],
      'xcode_settings': {
        'GCC_SYMBOLS_PRIVATE_EXTERN': 'YES',  # -fvisibility=hidden
      },
      'defines': [
        'JSON_USE_EXCEPTION=0'
      ],
      'include_dirs': [
        'jsoncpp/include',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'jsoncpp/include',
        ]
      },
      'sources': [
        'jsoncpp/src/lib_json/json_reader.cpp',
        'jsoncpp/src/lib_json/json_value.cpp',
        'jsoncpp/src/lib_json/json_writer.cpp',
      ]
    }
  ]
}
