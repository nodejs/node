{
  'includes': ['toolchain.gypi'],
  'targets': [
    {
      'target_name': 'highway',
      'type': 'static_library',
      'toolsets': ['host', 'target'],
      'variables': {
        'HIGHWAY_ROOT': '../../deps/v8/third_party/highway',
      },
      'all_dependent_settings': {
        'include_dirs': [
          '<(HIGHWAY_ROOT)/src',
        ],
        'conditions': [
          ['v8_target_arch=="ia32"', {
            'defines': ['HWY_BROKEN_TARGETS=(HWY_AVX2|HWY_AVX3)',],
          }],
          ['v8_target_arch=="arm64"', {
            'defines': ['HWY_BROKEN_TARGETS=HWY_ALL_SVE',],
          }],
          ['v8_target_arch=="ppc64" or v8_target_arch=="s390x"', {
            'defines': ['TOOLCHAIN_MISS_ASM_HWCAP_H',],
          }],
          ['v8_target_arch=="s390x"', {
            'defines': ['HWY_BROKEN_EMU128=0',],
          }],
          ['OS in "aix os400"', {
            'defines': ['HWY_BROKEN_EMU128=0',],
          }],
          ['v8_target_arch=="arm" and arm_version==7', {
            'defines': ['HWY_BROKEN_EMU128=0',],
          }],
        ],
      },
      'include_dirs': [
        '<(HIGHWAY_ROOT)/src',
      ],
      'conditions': [
        ['v8_target_arch=="ia32"', {
          'defines': ['HWY_BROKEN_TARGETS=(HWY_AVX2|HWY_AVX3)',],
        }],
        ['v8_target_arch=="arm64"', {
          'defines': ['HWY_BROKEN_TARGETS=HWY_ALL_SVE',],
        }],
        ['v8_target_arch=="ppc64" or v8_target_arch=="s390x"', {
          'defines': ['TOOLCHAIN_MISS_ASM_HWCAP_H',],
        }],
        ['v8_target_arch=="riscv64"', {
          'defines': ['HWY_BROKEN_TARGETS=HWY_RVV',],
        }],
      ],
      'sources': [
        '<!@pymod_do_main(GN-scraper "<(HIGHWAY_ROOT)/BUILD.gn"  "source_set.\\"libhwy.*?sources = ")',
      ],
    },  # highway
  ],
}
