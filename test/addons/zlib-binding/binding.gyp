{
  'targets': [
    {
      'target_name': 'binding',
      'variables': {
        # Skip this building on IBM i.
        'aix_variant_name': '<!(uname -s)',
      },
      'conditions': [
        [ '"<(aix_variant_name)"!="OS400"', {
          'sources': ['binding.cc'],
          'include_dirs': ['../../../deps/zlib'],
        }],
      ],
      'includes': ['../common.gypi'],
    },
  ]
}
