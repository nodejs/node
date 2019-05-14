# Note: The buildbots evaluate this file with CWD set to the parent
# directory and assume that the root of the checkout is in ./v8/, so
# all paths in here must match this assumption.

vars = {
  'checkout_instrumented_libraries': False,
  'chromium_url': 'https://chromium.googlesource.com',
  'download_gcmole': False,
  'download_jsfunfuzz': False,
  'download_mips_toolchain': False,
  'check_v8_header_includes': False,

  # luci-go CIPD package version.
  'luci_go': 'git_revision:25958d48e89e980e2a97daeddc977fb5e2e1fb8c',
}

deps = {
  'v8/build':
    Var('chromium_url') + '/chromium/src/build.git' + '@' + '80892bfe019dc854c6acdbfbb7304cca63986d4f',
  'v8/third_party/depot_tools':
    Var('chromium_url') + '/chromium/tools/depot_tools.git' + '@' + 'cf661acb705fccc302901a1f8a251ad43ce2dd62',
  'v8/third_party/icu':
    Var('chromium_url') + '/chromium/deps/icu.git' + '@' + '8c67416ccb4da42d817e7081ff83a2193b1aabe7',
  'v8/third_party/instrumented_libraries':
    Var('chromium_url') + '/chromium/src/third_party/instrumented_libraries.git' + '@' + 'a959e4f0cb643003f2d75d179cede449979e3e77',
  'v8/buildtools':
    Var('chromium_url') + '/chromium/src/buildtools.git' + '@' + '3e50219fc4503f461b2176a9976891b28d80f9ab',
  'v8/buildtools/clang_format/script':
    Var('chromium_url') + '/chromium/llvm-project/cfe/tools/clang-format.git' + '@' + '96636aa0e9f047f17447f2d45a094d0b59ed7917',
  'v8/buildtools/third_party/libc++/trunk':
    Var('chromium_url') + '/chromium/llvm-project/libcxx.git' + '@' + '22d3f6dd25e5efc59124ba1c00b8f98b14be4201',
  'v8/buildtools/third_party/libc++abi/trunk':
    Var('chromium_url') + '/chromium/llvm-project/libcxxabi.git' + '@' + '0d529660e32d77d9111912d73f2c74fc5fa2a858',
  'v8/buildtools/third_party/libunwind/trunk':
    Var('chromium_url') + '/external/llvm.org/libunwind.git' + '@' + '69d9b84cca8354117b9fe9705a4430d789ee599b',
  'v8/base/trace_event/common':
    Var('chromium_url') + '/chromium/src/base/trace_event/common.git' + '@' + '936ba8a963284a6b3737cf2f0474a7131073abee',
  'v8/third_party/android_ndk': {
    'url': Var('chromium_url') + '/android_ndk.git' + '@' + '4e2cea441bfd43f0863d14f57b1e1844260b9884',
    'condition': 'checkout_android',
  },
  'v8/third_party/android_tools': {
    'url': Var('chromium_url') + '/android_tools.git' + '@' + '347a7c8078a009e98995985b7ab6ec6b35696dea',
    'condition': 'checkout_android',
  },
  'v8/third_party/catapult': {
    'url': Var('chromium_url') + '/catapult.git' + '@' + 'ccc29087522abefc852d1294595ae6db7e86d649',
    'condition': 'checkout_android',
  },
  'v8/third_party/colorama/src': {
    'url': Var('chromium_url') + '/external/colorama.git' + '@' + '799604a1041e9b3bc5d2789ecbd7e8db2e18e6b8',
    'condition': 'checkout_android',
  },
  'v8/third_party/fuchsia-sdk': {
    'url': Var('chromium_url') + '/chromium/src/third_party/fuchsia-sdk.git' + '@' + '8e8db13b538ecb251e5ce9d5c781fc142f9752fd',
    'condition': 'checkout_fuchsia',
  },
  'v8/third_party/googletest/src':
    Var('chromium_url') + '/external/github.com/google/googletest.git' + '@' + 'efecb0bfa687cf87836494f5d62868485c00fb66',
  'v8/third_party/jinja2':
    Var('chromium_url') + '/chromium/src/third_party/jinja2.git' + '@' + 'b41863e42637544c2941b574c7877d3e1f663e25',
  'v8/third_party/markupsafe':
    Var('chromium_url') + '/chromium/src/third_party/markupsafe.git' + '@' + '8f45f5cfa0009d2a70589bcda0349b8cb2b72783',
  'v8/tools/swarming_client':
    Var('chromium_url') + '/infra/luci/client-py.git' + '@' + '7a61cf37d6a0163f0ec02d495289a1d038e62457',
  'v8/test/benchmarks/data':
    Var('chromium_url') + '/v8/deps/third_party/benchmarks.git' + '@' + '05d7188267b4560491ff9155c5ee13e207ecd65f',
  'v8/test/mozilla/data':
    Var('chromium_url') + '/v8/deps/third_party/mozilla-tests.git' + '@' + 'f6c578a10ea707b1a8ab0b88943fe5115ce2b9be',
  'v8/test/test262/data':
    Var('chromium_url') + '/external/github.com/tc39/test262.git' + '@' + '565d7d5b7dd808d9267006b83ac4ea9c48f782cc',
  'v8/test/test262/harness':
    Var('chromium_url') + '/external/github.com/test262-utils/test262-harness-py.git' + '@' + '0f2acdd882c84cff43b9d60df7574a1901e2cdcd',
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
  'v8/tools/clang':
    Var('chromium_url') + '/chromium/src/tools/clang.git' + '@' + '257c91cc44b07bd06ff03dde809ccbc46a22bec6',
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
  'v8/test/wasm-js/data':
    Var('chromium_url') + '/external/github.com/WebAssembly/spec.git' + '@' + '44dfa15cb87b1e9bef36e033ad5d2cdc4c2008fd',
}

recursedeps = [
  'v8/third_party/android_tools',
]

include_rules = [
  # Everybody can use some things.
  '+include',
  '+unicode',
  '+third_party/fdlibm',
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
  # Pull GN using checked-in hashes.
  {
    'name': 'gn_win',
    'pattern': '.',
    'condition': 'host_os == "win"',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--platform=win32',
                '--no_auth',
                '--bucket', 'chromium-gn',
                '-s', 'v8/buildtools/win/gn.exe.sha1',
    ],
  },
  {
    'name': 'gn_mac',
    'pattern': '.',
    'condition': 'host_os == "mac"',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--platform=darwin',
                '--no_auth',
                '--bucket', 'chromium-gn',
                '-s', 'v8/buildtools/mac/gn.sha1',
    ],
  },
  {
    'name': 'gn_linux',
    'pattern': '.',
    'condition': 'host_os == "linux"',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--platform=linux*',
                '--no_auth',
                '--bucket', 'chromium-gn',
                '-s', 'v8/buildtools/linux64/gn.sha1',
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
    'name': 'sysroot_mips',
    'pattern': '.',
    'condition': '(checkout_linux and checkout_mips)',
    'action': ['python', 'v8/build/linux/sysroot_scripts/install-sysroot.py',
               '--arch=mips'],
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
    'action': ['python', 'v8/tools/clang/scripts/download_lld_mac.py'],
  },
  {
      # Mac does not have llvm-objdump, download it for cross builds in Fuchsia.
    'name': 'llvm-objdump',
    'pattern': '.',
    'condition': 'host_os == "mac" and checkout_fuchsia',
    'action': ['python', 'v8/tools/clang/scripts/download_objdump.py'],
  },
  {
    'name': 'mips_toolchain',
    'pattern': '.',
    'condition': 'download_mips_toolchain',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--platform=linux',
                '--no_auth',
                '-u',
                '--bucket', 'chromium-v8',
                '-s', 'v8/tools/mips_toolchain.tar.gz.sha1',
    ],
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
