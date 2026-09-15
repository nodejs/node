{
  'variables': {
    'V8_ROOT': '../../deps/v8',
    'v8_code': 1,
  },
  'includes': ['toolchain.gypi', 'features.gypi'],
  'targets': [
    {
      # Intermediate target to build v8windbg.dll.
      # This prevents the dependent settings like node.gypi to link the v8windbg.dll
      # to the dependent. v8windbg.dll is only supposed to be loaded by WinDbg at debug time.
      'target_name': 'build_v8windbg',
      'type': 'none',
      'hard_dependency': 1,
      'dependencies': [
        'v8windbg',
      ],
    },  # build_v8windbg
    {
      'target_name': 'v8windbg',
      'type': 'shared_library',
      'include_dirs': [
        '<(V8_ROOT)',
        '<(V8_ROOT)/include',
      ],
      'dependencies': [
        'v8_debug_helper.gyp:v8_debug_helper',
        'v8.gyp:v8_libbase',
      ],
      'sources': [
        '<!@pymod_do_main(GN-scraper "<(V8_ROOT)/tools/v8windbg/BUILD.gn"  "v8windbg_base.*?sources = ")',
        '<!@pymod_do_main(GN-scraper "<(V8_ROOT)/tools/v8windbg/BUILD.gn"  "v8_shared_library..v8windbg.*?sources = ")',
      ],
      "link_settings": {
        "libraries": [
          "-lDbgEng.lib",
          "-lDbgModel.lib",
          "-lRuntimeObject.lib",
          "-lcomsuppwd.lib",
        ],
      },
    },  # v8windbg
  ],
}
