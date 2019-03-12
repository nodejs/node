{
  'variables': {
    'custom_malloc%' : 1,
  },
  'target_defaults': {
    'conditions': [
      ['custom_malloc==1', {
        'dependencies': [
          'malloc',
        ],
      }],
    ],
  },
  'targets': [
    {
      'target_name': 'main',
      'type': 'none',
      'dependencies': [ 'main_initial',],
    },
    {
      'target_name': 'main_initial',
      'type': 'executable',
      'product_name': 'main',
      'sources': [ 'main.c' ],
    },
    {
      'target_name': 'malloc',
      'type': 'shared_library',
      'variables': {
        'prune_self_dependency': 1,
        # Targets with type 'none' won't depend on this target.
        'link_dependency': 1,
      },  
      'sources': [ 'mymalloc.c' ],
    },
  ],
}
