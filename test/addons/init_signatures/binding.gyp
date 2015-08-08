{ 'target_defaults': { 'defines': ['NODE_TEST_ADDON_NAME=>(_target_name)']}
, 'targets':
  [ { 'target_name': 'init_exports'
    , 'sources'    : ['init_exports.cc']
    }
  , { 'target_name': 'init_exports_module'
    , 'sources'    : ['init_exports_module.cc']
    }
  , { 'target_name': 'init_exports_context'
    , 'sources'    : ['init_exports_context.cc']
    }
  , { 'target_name': 'init_exports_private'
    , 'sources'    : ['init_exports_private.cc']
    }
  , { 'target_name': 'init_exports_module_private'
    , 'sources'    : ['init_exports_module_private.cc']
    }
  , { 'target_name': 'init_exports_module_context'
    , 'sources'    : ['init_exports_module_context.cc']
    }
  , { 'target_name': 'init_exports_module_context_private'
    , 'sources'    : ['init_exports_module_context_private.cc']
    }
  ]
}
