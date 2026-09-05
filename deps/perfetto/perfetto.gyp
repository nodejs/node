{
  'variables': {
    'node_shared_perfetto%': 'false',
    'perfetto_sdk_sources': [
      'sdk/perfetto.cc',
      'sdk/perfetto.h',
    ]
  },
  'targets': [
    {
      'target_name': 'perfetto_sdk',
      'toolsets': ['host', 'target'],
      'conditions': [
        ['node_shared_perfetto=="true"', {
          # The SDK comes from the system, `include_dirs` and `libraries` are
          # provided by the configure script.
          'type': 'none',
        }, {
          'type': 'static_library',
          'include_dirs': [ 'sdk' ],
          'direct_dependent_settings': {
            # Use like `#include "perfetto.h"`
            'include_dirs': [ 'sdk' ],
          },
          'sources': [
            '<@(perfetto_sdk_sources)',
          ],
        }],
      ],
    },
  ]
}
