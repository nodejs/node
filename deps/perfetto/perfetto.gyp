{
  'variables': {
    'perfetto_sdk_sources': [
      'sdk/perfetto.cc',
      'sdk/perfetto.h',
    ]
  },
  'targets': [
    {
      'target_name': 'perfetto_sdk',
      'type': 'static_library',
      'include_dirs': [ 'sdk' ],
      'direct_dependent_settings': {
        # Use like `#include "perfetto.h"`
        'include_dirs': [ 'sdk' ],
      },
      'sources': [
        '<@(perfetto_sdk_sources)',
      ],
    },
  ]
}
