{
  'targets': [
    {
      'target_name': 'binding',
      'conditions': [
        ['OS in "aix os400"', {
          'variables': {
            # Used to differentiate `AIX` and `OS400`(IBM i).
            'aix_variant_name': '<!(uname -s)',
          },
          'conditions': [
            [ '"<(aix_variant_name)"!="OS400"', { # Not `OS400`(IBM i)
              'sources': ['binding.cc'],
              'include_dirs': ['../../../deps/zlib'],
            }],
          ],
        }, {
          'sources': ['binding.cc'],
          'include_dirs': ['../../../deps/zlib'],
        }],
      ],
      'includes': ['../common.gypi'],
    },
  ]
}
