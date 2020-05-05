# Note: The buildbots evaluate this file with CWD set to the parent
# directory and assume that the root of the checkout is in ./v8/, so
# all paths in here must match this assumption.

gclient_gn_args_file = 'v8/build/config/gclient_args.gni'
gclient_gn_args = [
]

vars = {
  # Fetches only the SDK boot images which match at least one of the whitelist
  # entries in a comma-separated list.
  #
  # Only the X64 and ARM64 QEMU images are downloaded by default. Developers
  # that need to boot on other target architectures or devices can opt to
  # download more boot images. Example of images include:
  #
  # Emulation:
  #   qemu.x64, qemu.arm64
  # Hardware:
  #   generic.x64, generic.arm64
  #
  # Wildcards are supported (e.g. "qemu.*").
  'checkout_fuchsia_boot_images': "qemu.x64,qemu.arm64",

  'checkout_instrumented_libraries': False,
  'checkout_ittapi': False,
  'chromium_url': 'https://chromium.googlesource.com',
  'android_url': 'https://android.googlesource.com',
  'download_gcmole': False,
  'download_jsfunfuzz': False,
  'check_v8_header_includes': False,

  # GN CIPD package version.
  'gn_version': 'git_revision:5ed3c9cc67b090d5e311e4bd2aba072173e82db9',

  # luci-go CIPD package version.
  'luci_go': 'git_revision:de73cf6c4bde86f0a9c8d54151b69b0154a398f1',

  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling android_sdk_build-tools_version
  # and whatever else without interference from each other.
  'android_sdk_build-tools_version': 'n-b1Qd7iFb8qzHlr1C_jIeu070UDgO_BwePtH42UqGcC',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling android_sdk_emulator_version
  # and whatever else without interference from each other.
  'android_sdk_emulator_version': 'f4WdgkPvDdVCE8zBWPzcSIj4N9WFhKp3CSKDWylXuLEC',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling android_sdk_extras_version
  # and whatever else without interference from each other.
  'android_sdk_extras_version': 'ppQ4TnqDvBHQ3lXx5KPq97egzF5X2FFyOrVHkGmiTMQC',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling android_sdk_patcher_version
  # and whatever else without interference from each other.
  'android_sdk_patcher_version': 'I6FNMhrXlpB-E1lOhMlvld7xt9lBVNOO83KIluXDyA0C',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling android_sdk_platform-tools_version
  # and whatever else without interference from each other.
  'android_sdk_platform-tools_version': 'Jxtur3_L9RzY4q79K-AwIahwFW4oi5uYVD5URx9h62wC',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling android_sdk_platforms_version
  # and whatever else without interference from each other.
  'android_sdk_platforms_version': 'yb33klKQV9UzzB-lDSsq36vzhTXOUZ2aRONBvPGwvdcC',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling android_sdk_sources_version
  # and whatever else without interference from each other.
  'android_sdk_sources_version': '4gxhM8E62bvZpQs7Q3d0DinQaW0RLCIefhXrQBFkNy8C',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling android_sdk_tools-lint_version
  # and whatever else without interference from each other.
  'android_sdk_tools-lint_version': '89hXqZYzCum3delB5RV7J_QyWkaRodqdtQS0s3LMh3wC',
}

deps = {
  'v8/build':
    Var('chromium_url') + '/chromium/src/build.git' + '@' + '26e9d485d01d6e0eb9dadd21df767a63494c8fea',
  'v8/third_party/depot_tools':
    Var('chromium_url') + '/chromium/tools/depot_tools.git' + '@' + '2b2aec6506a810f8d7bd018609de2c2450b3c121',
  'v8/third_party/icu':
    Var('chromium_url') + '/chromium/deps/icu.git' + '@' + 'd7aff76cf6bb0fbef3afa6c07718f78a80a70f8f',
  'v8/third_party/instrumented_libraries':
    Var('chromium_url') + '/chromium/src/third_party/instrumented_libraries.git' + '@' + 'bb3f1802c237dd19105dd0f7919f99e536a39d10',
  'v8/buildtools':
    Var('chromium_url') + '/chromium/src/buildtools.git' + '@' + '7977eb176752aeec29d888cfe8e677ac12ed1c41',
  'v8/buildtools/clang_format/script':
    Var('chromium_url') + '/chromium/llvm-project/cfe/tools/clang-format.git' + '@' + '96636aa0e9f047f17447f2d45a094d0b59ed7917',
  'v8/buildtools/linux64': {
    'packages': [
      {
        'package': 'gn/gn/linux-amd64',
        'version': Var('gn_version'),
      }
    ],
    'dep_type': 'cipd',
    'condition': 'host_os == "linux"',
  },
  'v8/buildtools/mac': {
    'packages': [
      {
        'package': 'gn/gn/mac-amd64',
        'version': Var('gn_version'),
      }
    ],
    'dep_type': 'cipd',
    'condition': 'host_os == "mac"',
  },
  'v8/buildtools/third_party/libc++/trunk':
    Var('chromium_url') + '/external/github.com/llvm/llvm-project/libcxx.git' + '@' + 'd9040c75cfea5928c804ab7c235fed06a63f743a',
  'v8/buildtools/third_party/libc++abi/trunk':
    Var('chromium_url') + '/external/github.com/llvm/llvm-project/libcxxabi.git' + '@' + '196ba1aaa8ac285d94f4ea8d9836390a45360533',
  'v8/buildtools/third_party/libunwind/trunk':
    Var('chromium_url') + '/external/github.com/llvm/llvm-project/libunwind.git' + '@' + '43bb9f872232f531bac80093ceb4de61c64b9ab7',
  'v8/buildtools/win': {
    'packages': [
      {
        'package': 'gn/gn/windows-amd64',
        'version': Var('gn_version'),
      }
    ],
    'dep_type': 'cipd',
    'condition': 'host_os == "win"',
  },
  'v8/base/trace_event/common':
    Var('chromium_url') + '/chromium/src/base/trace_event/common.git' + '@' + 'dab187b372fc17e51f5b9fad8201813d0aed5129',
  'v8/third_party/android_ndk': {
    'url': Var('chromium_url') + '/android_ndk.git' + '@' + '27c0a8d090c666a50e40fceb4ee5b40b1a2d3f87',
    'condition': 'checkout_android',
  },
  'v8/third_party/android_platform': {
    'url': Var('chromium_url') + '/chromium/src/third_party/android_platform.git' + '@' + '2244b5ea295f8fda3179bef160c84ef8fa0ec9fc',
    'condition': 'checkout_android',
  },
  'v8/third_party/android_sdk/public': {
      'packages': [
          {
              'package': 'chromium/third_party/android_sdk/public/build-tools/29.0.2',
              'version': Var('android_sdk_build-tools_version'),
          },
          {
              'package': 'chromium/third_party/android_sdk/public/emulator',
              'version': Var('android_sdk_emulator_version'),
          },
          {
              'package': 'chromium/third_party/android_sdk/public/extras',
              'version': Var('android_sdk_extras_version'),
          },
          {
              'package': 'chromium/third_party/android_sdk/public/patcher',
              'version': Var('android_sdk_patcher_version'),
          },
          {
              'package': 'chromium/third_party/android_sdk/public/platform-tools',
              'version': Var('android_sdk_platform-tools_version'),
          },
          {
              'package': 'chromium/third_party/android_sdk/public/platforms/android-29',
              'version': Var('android_sdk_platforms_version'),
          },
          {
              'package': 'chromium/third_party/android_sdk/public/sources/android-29',
              'version': Var('android_sdk_sources_version'),
          },
          {
              'package': 'chromium/third_party/android_sdk/public/tools-lint',
              'version': Var('android_sdk_tools-lint_version'),
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'v8/third_party/catapult': {
    'url': Var('chromium_url') + '/catapult.git' + '@' + '032c78376792ef343ea361bca2181ba6dec6b95f',
    'condition': 'checkout_android',
  },
  'v8/third_party/colorama/src': {
    'url': Var('chromium_url') + '/external/colorama.git' + '@' + '799604a1041e9b3bc5d2789ecbd7e8db2e18e6b8',
    'condition': 'checkout_android',
  },
  'v8/third_party/fuchsia-sdk': {
    'url': Var('chromium_url') + '/chromium/src/third_party/fuchsia-sdk.git' + '@' + '2457e41d8dc379f74662d3157e76339ba92cee06',
    'condition': 'checkout_fuchsia',
  },
  'v8/third_party/googletest/src':
    Var('chromium_url') + '/external/github.com/google/googletest.git' + '@' + '10b1902d893ea8cc43c69541d70868f91af3646b',
  'v8/third_party/jinja2':
    Var('chromium_url') + '/chromium/src/third_party/jinja2.git' + '@' + 'b41863e42637544c2941b574c7877d3e1f663e25',
  'v8/third_party/markupsafe':
    Var('chromium_url') + '/chromium/src/third_party/markupsafe.git' + '@' + '8f45f5cfa0009d2a70589bcda0349b8cb2b72783',
  'v8/tools/swarming_client':
    Var('chromium_url') + '/infra/luci/client-py.git' + '@' + 'cc958279ffd6853e0a1b227a7e957ca334fe56af',
  'v8/test/benchmarks/data':
    Var('chromium_url') + '/v8/deps/third_party/benchmarks.git' + '@' + '05d7188267b4560491ff9155c5ee13e207ecd65f',
  'v8/test/mozilla/data':
    Var('chromium_url') + '/v8/deps/third_party/mozilla-tests.git' + '@' + 'f6c578a10ea707b1a8ab0b88943fe5115ce2b9be',
  'v8/test/test262/data':
    Var('chromium_url') + '/external/github.com/tc39/test262.git' + '@' + 'f6b2ccdd091ff82da54150796297c3a96d7edb41',
  'v8/test/test262/harness':
    Var('chromium_url') + '/external/github.com/test262-utils/test262-harness-py.git' + '@' + '4555345a943d0c99a9461182705543fb171dda4b',
  'v8/third_party/qemu-linux-x64': {
      'packages': [
          {
              'package': 'fuchsia/qemu/linux-amd64',
              'version': '9cc486c5b18a0be515c39a280ca9a309c54cf994'
          },
      ],
      'condition': 'host_os == "linux" and checkout_fuchsia',
      'dep_type': 'cipd',
  },
  'v8/third_party/qemu-mac-x64': {
      'packages': [
          {
              'package': 'fuchsia/qemu/mac-amd64',
              'version': '2d3358ae9a569b2d4a474f498b32b202a152134f'
          },
      ],
      'condition': 'host_os == "mac" and checkout_fuchsia',
      'dep_type': 'cipd',
  },
  'v8/third_party/aemu-linux-x64': {
      'packages': [
          {
              'package': 'fuchsia/third_party/aemu/linux-amd64',
              'version': '7YlCgase5GlIanqHn-nZClSlZ5kQETJyVUYRF7Jjy6UC'
          },
      ],
      'condition': 'host_os == "linux" and checkout_fuchsia',
      'dep_type': 'cipd',
  },
  'v8/third_party/aemu-mac-x64': {
      'packages': [
          {
              'package': 'fuchsia/third_party/aemu/mac-amd64',
              'version': 'T9bWxf8aUC5TwCFgPxpuW29Mfy-7Z9xCfXB9QO8MfU0C'
          },
      ],
      'condition': 'host_os == "mac" and checkout_fuchsia',
      'dep_type': 'cipd',
  },
  'v8/tools/clang':
    Var('chromium_url') + '/chromium/src/tools/clang.git' + '@' + '105a8460911176861a422738eee4daad8dfe88a2',
  'v8/tools/luci-go': {
      'packages': [
        {
          'package': 'infra/tools/luci/isolate/${{platform}}',
          'version': Var('luci_go'),
        },
        {
          'package': 'infra/tools/luci/isolated/${{platform}}',
          'version': Var('luci_go'),
        },
        {
          'package': 'infra/tools/luci/swarming/${{platform}}',
          'version': Var('luci_go'),
        },
      ],
      'condition': 'host_cpu != "s390"',
      'dep_type': 'cipd',
  },
  'v8/tools/clang/dsymutil': {
    'packages': [
      {
        'package': 'chromium/llvm-build-tools/dsymutil',
        'version': 'M56jPzDv1620Rnm__jTMYS62Zi8rxHVq7yw0qeBFEgkC',
      }
    ],
    'condition': 'checkout_mac',
    'dep_type': 'cipd',
  },
  'v8/third_party/perfetto':
    Var('android_url') + '/platform/external/perfetto.git' + '@' + 'b9b24d1b0b80aafec393af085067e9eae829412f',
  'v8/third_party/protobuf':
    Var('chromium_url') + '/external/github.com/google/protobuf'+ '@' + 'b68a347f56137b4b1a746e8c7438495a6ac1bd91',
  'v8/third_party/zlib':
    Var('chromium_url') + '/chromium/src/third_party/zlib.git'+ '@' + '156be8c52f80cde343088b4a69a80579101b6e67',
  'v8/third_party/ittapi': {
    # Force checkout ittapi libraries to pass v8 header includes check on
    # bots that has check_v8_header_includes enabled.
    'url': Var('chromium_url') + '/external/github.com/intel/ittapi' + '@' + 'b4ae0122ba749163096058b4f1bb065bf4a7de94',
    'condition': "checkout_ittapi or check_v8_header_includes",
  },
}

include_rules = [
  # Everybody can use some things.
  '+include',
  '+unicode',
  '+third_party/fdlibm',
  '+third_party/ittapi/include'
]

# checkdeps.py shouldn't check for includes in these directories:
skip_child_includes = [
  'build',
  'third_party',
]

hooks = [
  {
    # Ensure that the DEPS'd "depot_tools" has its self-update capability
    # disabled.
    'name': 'disable_depot_tools_selfupdate',
    'pattern': '.',
    'action': [
        'python',
        'v8/third_party/depot_tools/update_depot_tools_toggle.py',
        '--disable',
    ],
  },
  {
    # This clobbers when necessary (based on get_landmines.py). It must be the
    # first hook so that other things that get/generate into the output
    # directory will not subsequently be clobbered.
    'name': 'landmines',
    'pattern': '.',
    'action': [
        'python',
        'v8/build/landmines.py',
        '--landmine-scripts',
        'v8/tools/get_landmines.py',
    ],
  },
  # Pull clang-format binaries using checked-in hashes.
  {
    'name': 'clang_format_win',
    'pattern': '.',
    'condition': 'host_os == "win"',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--platform=win32',
                '--no_auth',
                '--bucket', 'chromium-clang-format',
                '-s', 'v8/buildtools/win/clang-format.exe.sha1',
    ],
  },
  {
    'name': 'clang_format_mac',
    'pattern': '.',
    'condition': 'host_os == "mac"',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--platform=darwin',
                '--no_auth',
                '--bucket', 'chromium-clang-format',
                '-s', 'v8/buildtools/mac/clang-format.sha1',
    ],
  },
  {
    'name': 'clang_format_linux',
    'pattern': '.',
    'condition': 'host_os == "linux"',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--platform=linux*',
                '--no_auth',
                '--bucket', 'chromium-clang-format',
                '-s', 'v8/buildtools/linux64/clang-format.sha1',
    ],
  },
  {
    'name': 'gcmole',
    'pattern': '.',
    'condition': 'download_gcmole',
    'action': [ 'download_from_google_storage',
                '--bucket', 'chrome-v8-gcmole',
                '-u', '--no_resume',
                '-s', 'v8/tools/gcmole/gcmole-tools.tar.gz.sha1',
                '--platform=linux*',
    ],
  },
  {
    'name': 'jsfunfuzz',
    'pattern': '.',
    'condition': 'download_jsfunfuzz',
    'action': [ 'download_from_google_storage',
                '--bucket', 'chrome-v8-jsfunfuzz',
                '-u', '--no_resume',
                '-s', 'v8/tools/jsfunfuzz/jsfunfuzz.tar.gz.sha1',
                '--platform=linux*',
    ],
  },
  {
    'name': 'wasm_spec_tests',
    'pattern': '.',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--no_auth',
                '-u',
                '--bucket', 'v8-wasm-spec-tests',
                '-s', 'v8/test/wasm-spec-tests/tests.tar.gz.sha1',
    ],
  },
  {
    'name': 'wasm_js',
    'pattern': '.',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--no_auth',
                '-u',
                '--bucket', 'v8-wasm-spec-tests',
                '-s', 'v8/test/wasm-js/tests.tar.gz.sha1',
    ],
  },
  {
    'name': 'sysroot_arm',
    'pattern': '.',
    'condition': '(checkout_linux and checkout_arm)',
    'action': ['python', 'v8/build/linux/sysroot_scripts/install-sysroot.py',
               '--arch=arm'],
  },
  {
    'name': 'sysroot_arm64',
    'pattern': '.',
    'condition': '(checkout_linux and checkout_arm64)',
    'action': ['python', 'v8/build/linux/sysroot_scripts/install-sysroot.py',
               '--arch=arm64'],
  },
  {
    'name': 'sysroot_x86',
    'pattern': '.',
    'condition': '(checkout_linux and (checkout_x86 or checkout_x64))',
    'action': ['python', 'v8/build/linux/sysroot_scripts/install-sysroot.py',
               '--arch=x86'],
  },
  {
    'name': 'sysroot_x64',
    'pattern': '.',
    'condition': 'checkout_linux and checkout_x64',
    'action': ['python', 'v8/build/linux/sysroot_scripts/install-sysroot.py',
               '--arch=x64'],
  },
  {
    'name': 'msan_chained_origins',
    'pattern': '.',
    'condition': 'checkout_instrumented_libraries',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--no_auth',
                '--bucket', 'chromium-instrumented-libraries',
                '-s', 'v8/third_party/instrumented_libraries/binaries/msan-chained-origins-trusty.tgz.sha1',
              ],
  },
  {
    'name': 'msan_no_origins',
    'pattern': '.',
    'condition': 'checkout_instrumented_libraries',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--no_auth',
                '--bucket', 'chromium-instrumented-libraries',
                '-s', 'v8/third_party/instrumented_libraries/binaries/msan-no-origins-trusty.tgz.sha1',
              ],
  },
  {
    # Update the Windows toolchain if necessary.
    'name': 'win_toolchain',
    'pattern': '.',
    'condition': 'checkout_win',
    'action': ['python', 'v8/build/vs_toolchain.py', 'update'],
  },
  {
    # Update the Mac toolchain if necessary.
    'name': 'mac_toolchain',
    'pattern': '.',
    'condition': 'checkout_mac',
    'action': ['python', 'v8/build/mac_toolchain.py'],
  },
  # Pull binutils for linux, enabled debug fission for faster linking /
  # debugging when used with clang on Ubuntu Precise.
  # https://code.google.com/p/chromium/issues/detail?id=352046
  {
    'name': 'binutils',
    'pattern': 'v8/third_party/binutils',
    'condition': 'host_os == "linux"',
    'action': [
        'python',
        'v8/third_party/binutils/download.py',
    ],
  },
  {
    # Note: On Win, this should run after win_toolchain, as it may use it.
    'name': 'clang',
    'pattern': '.',
    # clang not supported on aix
    'condition': 'host_os != "aix"',
    'action': ['python', 'v8/tools/clang/scripts/update.py'],
  },
  {
    # Update LASTCHANGE.
    'name': 'lastchange',
    'pattern': '.',
    'action': ['python', 'v8/build/util/lastchange.py',
               '-o', 'v8/build/util/LASTCHANGE'],
  },
  {
    'name': 'fuchsia_sdk',
    'pattern': '.',
    'condition': 'checkout_fuchsia',
    'action': [
      'python',
      'v8/build/fuchsia/update_sdk.py',
      '--boot-images={checkout_fuchsia_boot_images}',
    ],
  },
  {
    # Mac doesn't use lld so it's not included in the default clang bundle
    # there. However, lld is need in Fuchsia cross builds, so
    # download it there.
    # Should run after the clang hook.
    'name': 'lld/mac',
    'pattern': '.',
    'condition': 'host_os == "mac" and checkout_fuchsia',
    'action': ['python', 'v8/tools/clang/scripts/update.py',
               '--package=lld_mac'],
  },
  {
      # Mac does not have llvm-objdump, download it for cross builds in Fuchsia.
    'name': 'llvm-objdump',
    'pattern': '.',
    'condition': 'host_os == "mac" and checkout_fuchsia',
    'action': ['python', 'v8/tools/clang/scripts/update.py',
               '--package=objdump'],
  },
  # Download and initialize "vpython" VirtualEnv environment packages.
  {
    'name': 'vpython_common',
    'pattern': '.',
    'condition': 'checkout_android',
    'action': [ 'vpython',
                '-vpython-spec', 'v8/.vpython',
                '-vpython-tool', 'install',
    ],
  },
  {
    'name': 'check_v8_header_includes',
    'pattern': '.',
    'condition': 'check_v8_header_includes',
    'action': [
      'python',
      'v8/tools/generate-header-include-checks.py',
    ],
  },
]
