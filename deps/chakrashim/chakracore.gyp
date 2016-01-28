{
  'variables': {
    'target_arch%': 'ia32',
    'library%': 'static_library',     # build chakracore as static library or dll
    'component%': 'static_library',   # link crt statically or dynamically
    'chakra_dir%': 'core',
    'msvs_windows_target_platform_version_prop': '',

    'conditions': [
      ['target_arch=="ia32"', { 'Platform': 'x86' }],
      ['target_arch=="x64"', { 'Platform': 'x64' }],
      ['target_arch=="arm"', {
        'Platform': 'arm',
        'msvs_windows_target_platform_version_prop':
          '/p:WindowsTargetPlatformVersion=$(WindowsTargetPlatformVersion)',
      }],
    ],
  },

  'targets': [
    {
      'target_name': 'chakracore',
      'toolsets': ['host'],
      'type': 'none',

      'variables': {
        'chakracore_sln': '<(chakra_dir)/build/Chakra.Core.sln',
        'chakracore_header': [
          '<(chakra_dir)/lib/jsrt/chakracore.h',
          '<(chakra_dir)/lib/jsrt/chakracommon.h'
        ],
        'chakracore_binaries': [
          '<(chakra_dir)/build/vcbuild/bin/<(Platform)_$(ConfigurationName)/chakracore.dll',
          '<(chakra_dir)/build/vcbuild/bin/<(Platform)_$(ConfigurationName)/chakracore.pdb',
          '<(chakra_dir)/build/vcbuild/bin/<(Platform)_$(ConfigurationName)/chakracore.lib',
        ],
      },

      'actions': [
        {
          'action_name': 'build_chakracore',
          'inputs': [
            '<(chakracore_sln)'
          ],
          'outputs': [
            '<@(chakracore_binaries)',
          ],
          'action': [
            'msbuild',
            '/p:Platform=<(Platform)',
            '/p:Configuration=$(ConfigurationName)',
            '/p:RuntimeLib=<(component)',
            '<(msvs_windows_target_platform_version_prop)',
            '/m',
            '<@(_inputs)',
          ],
        },
      ],

      'copies': [
        {
          'destination': 'include',
          'files': [ '<@(chakracore_header)' ],
        },
        {
          'destination': '<(PRODUCT_DIR)',
          'files': [ '<@(chakracore_binaries)' ],
        },
      ],

      'direct_dependent_settings': {
        'library_dirs': [ '<(PRODUCT_DIR)' ],
      },

    }, # end chakracore
  ],
}
