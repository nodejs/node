{
  'targets': [
    {
      'target_name': 'binding',
      'includes': ['../common.gypi'],
      'conditions': [
        ['node_use_openssl=="true"', {
          'include_dirs': ['<@(node_shared_openssl_include_dirs)'],
          'conditions': [
            ['OS in "aix os400"', {
              'variables': {
                # Used to differentiate `AIX` and `OS400`(IBM i).
                'aix_variant_name': '<!(uname -s)',
              },
              'conditions': [
                [ '"<(aix_variant_name)"=="OS400"', { # OS400 (IBM i): no sources, skip link
                  'type': 'none',
                }],
                [ '"<(aix_variant_name)"!="OS400"', { # Not `OS400`(IBM i)
                  'sources': ['binding.cc'],
                }],
              ],
            }, {
              'sources': ['binding.cc'],
            }],
          ],
        }],
      ],
    },
  ],
}
