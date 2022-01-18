# Note: The buildbots evaluate this file with CWD set to the parent
# directory and assume that the root of the checkout is in ./v8/, so
# all paths in here must match this assumption.

use_relative_paths = True

gclient_gn_args_file = 'build/config/gclient_args.gni'
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
  # Fetch clang-tidy into the same bin/ directory as our clang binary.
  'checkout_clang_tidy': False,
  'chromium_url': 'https://chromium.googlesource.com',
  'android_url': 'https://android.googlesource.com',
  'download_gcmole': False,
  'download_jsfunfuzz': False,
  'download_prebuilt_bazel': False,
  'check_v8_header_includes': False,
  'checkout_reclient': False,

  # reclient CIPD package version
  'reclient_version': 're_client_version:0.40.0.40ff5a5',

  # GN CIPD package version.
  'gn_version': 'git_revision:8926696a4186279489cc2b8d768533e61bba73d7',

  # luci-go CIPD package version.
  'luci_go': 'git_revision:68355732afb00a422ae0c70eed95c6a45f9868b1',

  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling android_sdk_build-tools_version
  # and whatever else without interference from each other.
  'android_sdk_build-tools_version': 'tRoD45SCi7UleQqSV7MrMQO1_e5P8ysphkCcj6z_cCQC',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling android_sdk_emulator_version
  # and whatever else without interference from each other.
  'android_sdk_emulator_version': 'gMHhUuoQRKfxr-MBn3fNNXZtkAVXtOwMwT7kfx8jkIgC',
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
  'android_sdk_platform-tools_version': 'g7n_-r6yJd_SGRklujGB1wEt8iyr77FZTUJVS9w6O34C',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling android_sdk_platforms_version
  # and whatever else without interference from each other.
  'android_sdk_platforms_version': 'lL3IGexKjYlwjO_1Ga-xwxgwbE_w-lmi2Zi1uOlWUIAC',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling android_sdk_sources_version
  # and whatever else without interference from each other.
  'android_sdk_sources_version': 'Yw53980aNNn0n9l58lN7u0wSVmxlY0OM1zFnGDQeJs4C',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling android_sdk_tools-lint_version
  # and whatever else without interference from each other.
  'android_sdk_cmdline-tools_version': 'AuYa11pULKT8AI14_owabJrkZoRGuovL-nvwmiONlYEC',
}

deps = {
  'base/trace_event/common':
    Var('chromium_url') + '/chromium/src/base/trace_event/common.git' + '@' + '7f36dbc19d31e2aad895c60261ca8f726442bfbb',
  'build':
    Var('chromium_url') + '/chromium/src/build.git' + '@' + 'cf325916d58a194a935c26a56fcf6b525d1e2bf4',
  'buildtools':
    Var('chromium_url') + '/chromium/src/buildtools.git' + '@' + '80e4f838faaf50e18629ae630df1d421f255a62a',
  'buildtools/clang_format/script':
    Var('chromium_url') + '/external/github.com/llvm/llvm-project/clang/tools/clang-format.git' + '@' + '99876cacf78329e5f99c244dbe42ccd1654517a0',
  'buildtools/linux64': {
    'packages': [
      {
        'package': 'gn/gn/linux-amd64',
        'version': Var('gn_version'),
      }
    ],
    'dep_type': 'cipd',
    'condition': 'host_os == "linux"',
  },
  'buildtools/mac': {
    'packages': [
      {
        'package': 'gn/gn/mac-${{arch}}',
        'version': Var('gn_version'),
      }
    ],
    'dep_type': 'cipd',
    'condition': 'host_os == "mac"',
  },
  'buildtools/third_party/libc++/trunk':
    Var('chromium_url') + '/external/github.com/llvm/llvm-project/libcxx.git' + '@' + '79a2e924d96e2fc1e4b937c42efd08898fa472d7',
  'buildtools/third_party/libc++abi/trunk':
    Var('chromium_url') + '/external/github.com/llvm/llvm-project/libcxxabi.git' + '@' + '4c6e0991b109638204f08c93600b008c21f01da5',
  'buildtools/third_party/libunwind/trunk':
    Var('chromium_url') + '/external/github.com/llvm/llvm-project/libunwind.git' + '@' + '99015718c37b30d44c3bcbcc92a03fb85fb85a99',
  'buildtools/win': {
    'packages': [
      {
        'package': 'gn/gn/windows-amd64',
        'version': Var('gn_version'),
      }
    ],
    'dep_type': 'cipd',
    'condition': 'host_os == "win"',
  },
  'buildtools/reclient': {
    'packages': [
      {
        'package': 'infra/rbe/client/${{platform}}',
        'version': Var('reclient_version'),
      }
    ],
    'dep_type': 'cipd',
    'condition': '(host_os == "linux" or host_os == "win") and checkout_reclient',
  },
  'test/benchmarks/data':
    Var('chromium_url') + '/v8/deps/third_party/benchmarks.git' + '@' + '05d7188267b4560491ff9155c5ee13e207ecd65f',
  'test/mozilla/data':
    Var('chromium_url') + '/v8/deps/third_party/mozilla-tests.git' + '@' + 'f6c578a10ea707b1a8ab0b88943fe5115ce2b9be',
  'test/test262/data':
    Var('chromium_url') + '/external/github.com/tc39/test262.git' + '@' + 'ba82d46238bd16c3e31b93d21d2846c81a9ccf7a',
  'test/test262/harness':
    Var('chromium_url') + '/external/github.com/test262-utils/test262-harness-py.git' + '@' + '278bcfaed0dcaa13936831fb1769d15e7c1e3b2b',
  'third_party/aemu-linux-x64': {
      'packages': [
          {
              'package': 'fuchsia/third_party/aemu/linux-amd64',
              'version': 'hys6gk1KOHMz9nURGWen255HiLIaVd3e4eZfa-w6l7oC'
          },
      ],
      'condition': 'host_os == "linux" and checkout_fuchsia',
      'dep_type': 'cipd',
  },
  'third_party/aemu-mac-x64': {
      'packages': [
          {
              'package': 'fuchsia/third_party/aemu/mac-amd64',
              'version': 'T9bWxf8aUC5TwCFgPxpuW29Mfy-7Z9xCfXB9QO8MfU0C'
          },
      ],
      'condition': 'host_os == "mac" and checkout_fuchsia',
      'dep_type': 'cipd',
  },
  'third_party/android_ndk': {
    'url': Var('chromium_url') + '/android_ndk.git' + '@' + '401019bf85744311b26c88ced255cd53401af8b7',
    'condition': 'checkout_android',
  },
  'third_party/android_platform': {
    'url': Var('chromium_url') + '/chromium/src/third_party/android_platform.git' + '@' + '72e09e98a62744cd10b762bd438c702ed8b131fb',
    'condition': 'checkout_android',
  },
  'third_party/android_sdk/public': {
      'packages': [
          {
              'package': 'chromium/third_party/android_sdk/public/build-tools/31.0.0',
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
              'package': 'chromium/third_party/android_sdk/public/platforms/android-31',
              'version': Var('android_sdk_platforms_version'),
          },
          {
              'package': 'chromium/third_party/android_sdk/public/sources/android-30',
              'version': Var('android_sdk_sources_version'),
          },
          {
              'package': 'chromium/third_party/android_sdk/public/cmdline-tools',
              'version': Var('android_sdk_cmdline-tools_version'),
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'third_party/catapult': {
    'url': Var('chromium_url') + '/catapult.git' + '@' + '75c4ea8c6eef1d5941ec3d5cfee174e8d0f73566',
    'condition': 'checkout_android',
  },
  'third_party/colorama/src': {
    'url': Var('chromium_url') + '/external/colorama.git' + '@' + '799604a1041e9b3bc5d2789ecbd7e8db2e18e6b8',
    'condition': 'checkout_android',
  },
  'third_party/depot_tools':
    Var('chromium_url') + '/chromium/tools/depot_tools.git' + '@' + '57c928cd959aa46e9dbd6b0bc754888075b4a4c3',
  'third_party/fuchsia-sdk': {
    'url': Var('chromium_url') + '/chromium/src/third_party/fuchsia-sdk.git' + '@' + '18896843130c33372c455c153ad07d2217bd2085',
    'condition': 'checkout_fuchsia',
  },
  'third_party/google_benchmark/src': {
    'url': Var('chromium_url') + '/external/github.com/google/benchmark.git' + '@' + '4f31803ebbf283e24602260e39e63a296d44b0f8',
  },
  'third_party/googletest/src':
    Var('chromium_url') + '/external/github.com/google/googletest.git' + '@' + '16f637fbf4ffc3f7a01fa4eceb7906634565242f',
  'third_party/icu':
    Var('chromium_url') + '/chromium/deps/icu.git' + '@' + 'eedbaf76e49d28465d9119b10c30b82906e606ff',
  'third_party/instrumented_libraries':
    Var('chromium_url') + '/chromium/src/third_party/instrumented_libraries.git' + '@' + '3c149f5611237dc59a7ec229e8ea009d8be8f51d',
  'third_party/ittapi': {
    # Force checkout ittapi libraries to pass v8 header includes check on
    # bots that has check_v8_header_includes enabled.
    'url': Var('chromium_url') + '/external/github.com/intel/ittapi' + '@' + 'a3911fff01a775023a06af8754f9ec1e5977dd97',
    'condition': "checkout_ittapi or check_v8_header_includes",
  },
  'third_party/jinja2':
    Var('chromium_url') + '/chromium/src/third_party/jinja2.git' + '@' + 'ee69aa00ee8536f61db6a451f3858745cf587de6',
  'third_party/jsoncpp/source':
    Var('chromium_url') + '/external/github.com/open-source-parsers/jsoncpp.git'+ '@' + '9059f5cad030ba11d37818847443a53918c327b1',
  'third_party/logdog/logdog':
    Var('chromium_url') + '/infra/luci/luci-py/client/libs/logdog' + '@' + '17ec234f823f7bff6ada6584fdbbee9d54b8fc58',
  'third_party/markupsafe':
    Var('chromium_url') + '/chromium/src/third_party/markupsafe.git' + '@' + '1b882ef6372b58bfd55a3285f37ed801be9137cd',
  'third_party/perfetto':
    Var('android_url') + '/platform/external/perfetto.git' + '@' + 'aa4385bc5997ecad4c633885e1b331b1115012fb',
  'third_party/protobuf':
    Var('chromium_url') + '/external/github.com/google/protobuf'+ '@' + '6a59a2ad1f61d9696092f79b6d74368b4d7970a3',
  'third_party/qemu-linux-x64': {
      'packages': [
          {
              'package': 'fuchsia/qemu/linux-amd64',
              'version': '9cc486c5b18a0be515c39a280ca9a309c54cf994'
          },
      ],
      'condition': 'host_os == "linux" and checkout_fuchsia',
      'dep_type': 'cipd',
  },
  'third_party/qemu-mac-x64': {
      'packages': [
          {
              'package': 'fuchsia/qemu/mac-amd64',
              'version': '2d3358ae9a569b2d4a474f498b32b202a152134f'
          },
      ],
      'condition': 'host_os == "mac" and checkout_fuchsia',
      'dep_type': 'cipd',
  },
  'third_party/requests': {
      'url': Var('chromium_url') + '/external/github.com/kennethreitz/requests.git' + '@' + '2c2138e811487b13020eb331482fb991fd399d4e',
      'condition': 'checkout_android',
  },
  'third_party/zlib':
    Var('chromium_url') + '/chromium/src/third_party/zlib.git'+ '@' + '6da1d53b97c89b07e47714d88cab61f1ce003c68',
  'tools/clang':
    Var('chromium_url') + '/chromium/src/tools/clang.git' + '@' + '21baac0e13389b03d6f805701c75544ed0b1ebb0',
  'tools/clang/dsymutil': {
    'packages': [
      {
        'package': 'chromium/llvm-build-tools/dsymutil',
        'version': 'M56jPzDv1620Rnm__jTMYS62Zi8rxHVq7yw0qeBFEgkC',
      }
    ],
    'condition': 'checkout_mac',
    'dep_type': 'cipd',
  },
  'tools/luci-go': {
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
      'condition': 'host_cpu != "s390" and host_os != "aix"',
      'dep_type': 'cipd',
  },
}

include_rules = [
  # Everybody can use some things.
  '+include',
  '+unicode',
  '+third_party/fdlibm',
  '+third_party/ittapi/include',
  # Abseil features are allow-listed. Please use your best judgement when adding
  # to this set -- if in doubt, email v8-dev@. For general guidance, refer to
  # the Chromium guidelines (though note that some requirements in V8 may be
  # different to Chromium's):
  # https://chromium.googlesource.com/chromium/src/+/main/styleguide/c++/c++11.md
  '+absl/types/optional.h',
  '+absl/types/variant.h',
  '+absl/status',
  # Some abseil features are explicitly banned.
  '-absl/types/any.h', # Requires RTTI.
  '-absl/types/flags', # Requires RTTI.
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
        'third_party/depot_tools/update_depot_tools_toggle.py',
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
        'build/landmines.py',
        '--landmine-scripts',
        'tools/get_landmines.py',
    ],
  },
  {
    'name': 'bazel',
    'pattern': '.',
    'condition': 'download_prebuilt_bazel',
    'action': [ 'download_from_google_storage',
                '--bucket', 'chromium-v8-prebuilt-bazel/linux',
                '--no_resume',
                '-s', 'tools/bazel/bazel.sha1',
                '--platform=linux*',
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
                '-s', 'buildtools/win/clang-format.exe.sha1',
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
                '-s', 'buildtools/mac/clang-format.sha1',
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
                '-s', 'buildtools/linux64/clang-format.sha1',
    ],
  },
  {
    'name': 'gcmole',
    'pattern': '.',
    'condition': 'download_gcmole',
    'action': [ 'download_from_google_storage',
                '--bucket', 'chrome-v8-gcmole',
                '-u', '--no_resume',
                '-s', 'tools/gcmole/gcmole-tools.tar.gz.sha1',
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
                '-s', 'tools/jsfunfuzz/jsfunfuzz.tar.gz.sha1',
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
                '-s', 'test/wasm-spec-tests/tests.tar.gz.sha1',
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
                '-s', 'test/wasm-js/tests.tar.gz.sha1',
    ],
  },
  {
    'name': 'sysroot_arm',
    'pattern': '.',
    'condition': '(checkout_linux and checkout_arm)',
    'action': ['python', 'build/linux/sysroot_scripts/install-sysroot.py',
               '--arch=arm'],
  },
  {
    'name': 'sysroot_arm64',
    'pattern': '.',
    'condition': '(checkout_linux and checkout_arm64)',
    'action': ['python', 'build/linux/sysroot_scripts/install-sysroot.py',
               '--arch=arm64'],
  },
  {
    'name': 'sysroot_x86',
    'pattern': '.',
    'condition': '(checkout_linux and (checkout_x86 or checkout_x64))',
    'action': ['python', 'build/linux/sysroot_scripts/install-sysroot.py',
               '--arch=x86'],
  },
  {
    'name': 'sysroot_x64',
    'pattern': '.',
    'condition': 'checkout_linux and checkout_x64',
    'action': ['python', 'build/linux/sysroot_scripts/install-sysroot.py',
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
                '-s', 'third_party/instrumented_libraries/binaries/msan-chained-origins.tgz.sha1',
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
                '-s', 'third_party/instrumented_libraries/binaries/msan-no-origins.tgz.sha1',
              ],
  },
  {
    # Case-insensitivity for the Win SDK. Must run before win_toolchain below.
    'name': 'ciopfs_linux',
    'pattern': '.',
    'condition': 'checkout_win and host_os == "linux"',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--no_auth',
                '--bucket', 'chromium-browser-clang/ciopfs',
                '-s', 'build/ciopfs.sha1',
    ]
  },
  {
    # Update the Windows toolchain if necessary.
    'name': 'win_toolchain',
    'pattern': '.',
    'condition': 'checkout_win',
    'action': ['python', 'build/vs_toolchain.py', 'update', '--force'],
  },
  {
    # Update the Mac toolchain if necessary.
    'name': 'mac_toolchain',
    'pattern': '.',
    'condition': 'checkout_mac',
    'action': ['python', 'build/mac_toolchain.py'],
  },
  {
    # Note: On Win, this should run after win_toolchain, as it may use it.
    'name': 'clang',
    'pattern': '.',
    # clang not supported on aix
    'condition': 'host_os != "aix"',
    'action': ['python3', 'tools/clang/scripts/update.py'],
  },
  {
    'name': 'clang_tidy',
    'pattern': '.',
    'condition': 'checkout_clang_tidy',
    'action': ['python3', 'tools/clang/scripts/update.py',
               '--package=clang-tidy'],
  },
  {
    # Update LASTCHANGE.
    'name': 'lastchange',
    'pattern': '.',
    'action': ['python', 'build/util/lastchange.py',
               '-o', 'build/util/LASTCHANGE'],
  },
  {
    'name': 'Download Fuchsia SDK',
    'pattern': '.',
    'condition': 'checkout_fuchsia',
    'action': [
      'python',
      'build/fuchsia/update_sdk.py',
    ],
  },
  {
    'name': 'Download Fuchsia system images',
    'pattern': '.',
    'condition': 'checkout_fuchsia',
    'action': [
      'python',
      'build/fuchsia/update_images.py',
      '--boot-images={checkout_fuchsia_boot_images}',
    ],
  },
  {
    # Mac does not have llvm-objdump, download it for cross builds in Fuchsia.
    'name': 'llvm-objdump',
    'pattern': '.',
    'condition': 'host_os == "mac" and checkout_fuchsia',
    'action': ['python3', 'tools/clang/scripts/update.py',
               '--package=objdump'],
  },
  # Download and initialize "vpython" VirtualEnv environment packages.
  {
    'name': 'vpython_common',
    'pattern': '.',
    'condition': 'checkout_android',
    'action': [ 'vpython',
                '-vpython-spec', '.vpython',
                '-vpython-tool', 'install',
    ],
  },
  {
    'name': 'check_v8_header_includes',
    'pattern': '.',
    'condition': 'check_v8_header_includes',
    'action': [
      'python',
      'tools/generate-header-include-checks.py',
    ],
  },
]
