{
  'includes': ['../../../config.gypi'],
  'variables': {
    'node_target_type%': '',
  },
  'targets': [
    {
      'target_name': 'binding',
      'sources': ['binding.cc'],
      'include_dirs': ['../../../deps/zlib'],
      'conditions': [
        ['node_target_type=="static_library"', {
          'conditions': [
            ['OS=="win"', {
	      'libraries': ['../../../../$(Configuration)/lib/zlib.lib'],
            }],
          ],
        }],
      ],
    },
  ]
}
