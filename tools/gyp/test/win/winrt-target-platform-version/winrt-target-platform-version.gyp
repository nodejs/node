{
 'targets': [
    {
      'target_name': 'enable_winrt_10_platversion_dll',
      'type': 'shared_library',
      'msvs_enable_winrt': 1,
      'msvs_application_type_revision': '10.0',
      'msvs_target_platform_version':'10.0.10240.0',
      'msvs_target_platform_minversion':'10.0.10240.0',
      'sources': [
        'dllmain.cc',
      ],
    },
    {
      'target_name': 'enable_winrt_10_platversion_nominver_dll',
      'type': 'shared_library',
      'msvs_enable_winrt': 1,
      'msvs_application_type_revision': '10.0',
      'msvs_target_platform_version':'10.0.10240.0',
      'sources': [
        'dllmain.cc',
      ],
    },
    {
      'target_name': 'enable_winrt_9_platversion_dll',
      'type': 'shared_library',
      'msvs_enable_winrt': 1,
      'msvs_application_type_revision': '10.0',
      'msvs_target_platform_version':'9.0.0.0',
      'msvs_target_platform_minversion':'9.0.0.0',
      'sources': [
        'dllmain.cc',
      ],
    },
    {
      'target_name': 'enable_winrt_missing_platversion_dll',
      'type': 'shared_library',
      'msvs_enable_winrt': 1,
      'msvs_application_type_revision': '10.0',
      'sources': [
        'dllmain.cc',
      ],
    },
  ]
}
