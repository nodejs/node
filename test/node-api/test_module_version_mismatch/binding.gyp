{
  'targets': [
    {
      'target_name': 'test_module_version_mismatch',
      'sources': [ 'test_module_version_mismatch.c' ],
      # One below NAPI_VERSION_EXPERIMENTAL, so it is always above
      # NODE_API_SUPPORTED_VERSION_MAX and never becomes a real version, but is
      # not the experimental value the version check deliberately allows.
      'defines': [ 'NAPI_VERSION=2147483646' ]
    }
  ]
}
