# Note: The buildbots evaluate this file with CWD set to the parent
# directory and assume that the root of the checkout is in ./v8/, so
# all paths in here must match this assumption.

use_relative_paths = True

gclient_gn_args_file = 'build/config/gclient_args.gni'
gclient_gn_args = [
  'android_ndk_version',
  'checkout_src_internal',
]

vars = {
  # The version of the NDK. Set here, to allow the autoroller to update this
  # value when updating the CIPD hash.
  'android_ndk_version': Str('2@30.0.16248370'),

  # Fetches only the SDK boot images which match at least one of the whitelist
  # entries in a comma-separated list.
  #
  # Available images:
  #   Emulation:
  #   - terminal.qemu-x64
  #   - terminal.qemu-arm64
  #   - workstation.qemu-x64-release
  #   Hardware:
  #   - minimal.x64
  #   - core.x64-dfv2
  #
  # Since the images are hundreds of MB, default to only downloading the image
  # most commonly useful for developers. Bots and developers that need to use
  # other images (e.g., qemu.arm64) can override this with additional images.
  'checkout_fuchsia_boot_images': "terminal.x64",
  'checkout_fuchsia_product_bundles': '"{checkout_fuchsia_boot_images}" != ""',

  'checkout_instrumented_libraries': False,
  'checkout_ittapi': False,
  # Checkout extra benchmarks.
  'checkout_benchmarks': False,

  # Fetch the internal v8 perf repository for additional benchmarks.
  'checkout_v8_perf': False,

  # Fetch the prebuilt binaries for llvm-cov and llvm-profdata. Needed to
  # process the raw profiles produced by instrumented targets (built with
  # the gn arg 'use_clang_coverage').
  'checkout_clang_coverage_tools': False,

  # Fetch clang-tidy into the same bin/ directory as our clang binary.
  'checkout_clang_tidy': False,

  # Fetch clangd into the same bin/ directory as our clang binary.
  'checkout_clangd': False,

  # Fetch and build V8 builtins with PGO profiles
  'checkout_v8_builtins_pgo_profiles': False,

  'android_url': 'https://android.googlesource.com',
  'boringssl_url': 'https://boringssl.googlesource.com',
  'chromium_url': 'https://chromium.googlesource.com',
  'chrome_internal_url': 'https://chrome-internal.googlesource.com',
  'download_gcmole': False,
  'download_jsfunfuzz': False,
  'download_prebuilt_bazel': False,
  'download_prebuilt_arm64_llvm_symbolizer': False,
  'check_v8_header_includes': False,

  # By default, download the fuchsia sdk from the public sdk directory.
  'fuchsia_sdk_cipd_prefix': 'fuchsia/sdk/core/',

  # Used for downloading the Fuchsia SDK without running hooks.
  'checkout_fuchsia_no_hooks': False,

  # V8 doesn't need src_internal, but some shared GN files use this variable.
  'checkout_src_internal': False,

  # Fetch the internal agents repository.
  'checkout_agents_internal': False,

  # CPython 3 CIPD package version for Siso hermetic toolchain.
  'cpython3_version': 'version:3@3.11.9.chromium.38',

  # Fetch configuration files required for the 'use_remoteexec' gn arg
  'download_remoteexec_cfg': False,

  # RBE instance to use for running remote builds
  'rbe_instance': Str('projects/rbe-chrome-untrusted/instances/default_instance'),

  # RBE project to download rewrapper config files for. Only needed if
  # different from the project used in 'rbe_instance'
  'rewrapper_cfg_project': Str(''),

  # This variable is overrided in Chromium's DEPS file.
  'build_with_chromium': False,

  # Repository URL
  'chromium_jetstream_git': 'https://chromium.googlesource.com/external/github.com/WebKit/JetStream.git',

  # GN CIPD package version.
  'gn_version': 'git_revision:ddc6c4ae595393d43b38e62f2e2baa7661af716f',

  # ninja CIPD package version
  # https://chrome-infra-packages.appspot.com/p/infra/3pp/tools/ninja
  'ninja_version': 'version:3@1.12.1.chromium.4',

  # siso CIPD package version
  'siso_version': 'git_revision:3025be9c477069426f08b8235e9545d5b20afd0f',

  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling Fuchsia sdk
  # and whatever else without interference from each other.
  'fuchsia_version': 'version:34.20261029.4.1',

  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling partition_alloc_version
  # and whatever else without interference from each other.
  'partition_alloc_version': '01906ada20b59e7e9a690a75ae7830f4080b3a78',

  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling android_sdk_build-tools_version
  # and whatever else without interference from each other.
  'android_sdk_build-tools_version': 'version_37.0.0',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling android_sdk_emulator_version
  # and whatever else without interference from each other.
  'android_sdk_emulator_version': '9lGp8nTUCRRWGMnI_96HcKfzjnxEJKUcfvfwmA3wXNkC',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling android_sdk_platform-tools_version
  # and whatever else without interference from each other.
  'android_sdk_platform-tools_version': 'qTD9QdBlBf3dyHsN1lJ0RH6AhHxR42Hmg2Ih-Vj4zIEC',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling android_sdk_platforms_version
  # and whatever else without interference from each other.
  'android_sdk_platforms_version': 'JsUGMsJK2pkQxVKr7EcJbxbwjJEIWjEPLqcbv7ifdmcC',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling android_sdk_tools-lint_version
  # and whatever else without interference from each other.
  'android_sdk_cmdline-tools_version': 'gekOVsZjseS1w9BXAT3FsoW__ByGDJYS9DgqesiwKYoC',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling jetstream_3.0-custom_revision
  # and whatever else without interference from each other.
  'jetstream_3.0-custom_revision': 'ffde597c189c9c781986a6247a5844b1c04e4a8a',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling agents-common
  # and whatever else without interference from each other.
  'agents_common_revision': '281f7364f86aef21ac620bc0b770d477de7c76a1',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling agents-internal
  # and whatever else without interference from each other.
  'agents_internal_revision': '5bbf8f7c3b5cbc477770c7a63071a2ca4937b35b',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling agents-public
  # and whatever else without interference from each other.
  'agents_public_revision': 'f3fecac8a714ae5512abb3a635387c7a01da8666',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling agents-testing
  # and whatever else without interference from each other.
  'agents_testing_revision': 'b1f0d9a76768b01c6b17f0e123768baffd84e24d',
}

deps = {
  'agents/common': {
    'url': Var('chromium_url') + '/chromium/src/agents/common.git' + '@' + Var('agents_common_revision'),
  },
  'agents/shared': {
    'url': Var('chromium_url') + '/chromium/agents.git' + '@' + Var('agents_public_revision'),
  },
  'agents/testing': {
    'url': Var('chromium_url') + '/chromium/src/agents/testing.git' + '@' + Var('agents_testing_revision'),
  },
  'agents/internal': {
    'url': Var('chrome_internal_url') + '/chrome/agents-internal.git' + '@' + Var('agents_internal_revision'),
    'condition': 'checkout_agents_internal',
  },
  'build':
    Var('chromium_url') + '/chromium/src/build.git' + '@' + 'e5270b6d8780c5e3c9e4175bd0ae5b357fa2a5d1',
  'buildtools':
    Var('chromium_url') + '/chromium/src/buildtools.git' + '@' + '8ffdd1068ab0ebe98e082845b494b18be5f3ec2f',
  'buildtools/linux64': {
    'packages': [
      {
        'package': 'gn/gn/linux-${{arch}}',
        'version': Var('gn_version'),
      }
    ],
    'dep_type': 'cipd',
    'condition': 'host_os == "linux" and host_cpu != "s390x" and host_os != "zos" and host_cpu != "ppc64"',
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
  # TODO(498118202): Use checkout_benchmarks here too.
  'test/benchmarks/data':
    Var('chromium_url') + '/v8/deps/third_party/benchmarks.git' + '@' + '05d7188267b4560491ff9155c5ee13e207ecd65f',
  'test/benchmarks/JetStream3': {
    'url': Var('chromium_jetstream_git') + '@' + Var('jetstream_3.0-custom_revision'),
    'condition': 'checkout_benchmarks',
  },
  'third_party/v8-perf': {
    'url': Var('chrome_internal_url') + '/v8/v8-perf.git' + '@' + 'HEAD',
    'condition': 'checkout_v8_perf',
  },
  'test/mozilla/data':
    Var('chromium_url') + '/v8/deps/third_party/mozilla-tests.git' + '@' + 'f6c578a10ea707b1a8ab0b88943fe5115ce2b9be',
  'test/test262/data':
    Var('chromium_url') + '/external/github.com/tc39/test262.git' + '@' + '2e58f87d8350ad51224617a041c6e041d52f8444',
  'third_party/android_platform': {
    'url': Var('chromium_url') + '/chromium/src/third_party/android_platform.git' + '@' + '4cabfaca9bdfe8d1711fe4954a67f435eb3f4d09',
    'condition': 'checkout_android',
  },
  'third_party/android_sdk/public': {
      'packages': [
          {
              'package': 'chromium/third_party/android_sdk/public/build-tools/37.0.0/${{os}}',
              'version': Var('android_sdk_build-tools_version'),
          },
          {
              'package': 'chromium/third_party/android_sdk/public/emulator',
              'version': Var('android_sdk_emulator_version'),
          },
          {
              'package': 'chromium/third_party/android_sdk/public/platform-tools',
              'version': Var('android_sdk_platform-tools_version'),
          },
          {
              'package': 'chromium/third_party/android_sdk/public/platforms/android-37.0',
              'version': Var('android_sdk_platforms_version'),
          },
          {
              'package': 'chromium/third_party/android_sdk/public/cmdline-tools',
              'version': Var('android_sdk_cmdline-tools_version'),
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'third_party/android_toolchain/ndk': {
    'packages': [
      {
        'package': 'chromium/third_party/android_toolchain/android_toolchain/${{platform}}',
        'version': 'version:' + Var('android_ndk_version'),
      },
    ],
    'condition': 'checkout_android and '
                 '((host_os == "linux" and host_cpu == "x64") or '
                 '(host_os == "mac" and host_cpu == "arm64"))',
    'dep_type': 'cipd',
  },
  'third_party/catapult': {
    'url': Var('chromium_url') + '/catapult.git' + '@' + '6e8c156cb5743122a3bea59a0665384c52f7e00b',
    'condition': 'checkout_android',
  },
  'third_party/clang-format/script':
    Var('chromium_url') + '/external/github.com/llvm/llvm-project/clang/tools/clang-format.git' + '@' + '0ca48e889615842946cdcc09c4e7ee976dc36444',
  'third_party/colorama/src': {
    'url': Var('chromium_url') + '/external/colorama.git' + '@' + '3de9f013df4b470069d03d250224062e8cf15c49',
    'condition': 'checkout_android',
  },
  'third_party/cpu_features': {
    'url': Var('chromium_url') + '/chromium/src/third_party/cpu_features.git' + '@' + '378c196c1dc411059833778ab6b2ab2801f8c9ab',
    'condition': 'checkout_android',
  },
  'third_party/cpu_features/src': {
    'url': Var('chromium_url') + '/external/github.com/google/cpu_features.git' + '@' + '81d13c49649f0714dd41fb56bb246398b6584085',
    'condition': 'checkout_android',
  },
  'third_party/depot_tools':
    Var('chromium_url') + '/chromium/tools/depot_tools.git' + '@' + 'fed8b43f48c4f4c2ac0c34aafbf0ec07a4e600cb',
  'third_party/dragonbox/src':
    Var('chromium_url') + '/external/github.com/jk-jeon/dragonbox.git' + '@' + 'beeeef91cf6fef89a4d4ba5e95d47ca64ccb3a44',
  'third_party/fp16/src':
    Var('chromium_url') + '/external/github.com/Maratyszcza/FP16.git' + '@' + '782eea126dc5c755827be751a099eb01826175cf',
  'third_party/fast_float/src':
    Var('chromium_url') + '/external/github.com/fastfloat/fast_float.git' + '@' + 'dbeabeeb3e6791cb96994c2a7a084c36e62ccfb2',
  'third_party/fuchsia-gn-sdk': {
    'url': Var('chromium_url') + '/chromium/src/third_party/fuchsia-gn-sdk.git' + '@' + '8488f5b23e737d08f2888cf0bb0d55fe7d313092',
    'condition': 'checkout_fuchsia',
  },
  'third_party/simdutf':
    Var('chromium_url') + '/chromium/src/third_party/simdutf' + '@' + 'f7356eed293f8208c40b3c1b344a50bd70971983',
  # Exists for rolling the Fuchsia SDK. Check out of the SDK should always
  # rely on the hook running |update_sdk.py| script below.
  'third_party/fuchsia-sdk/sdk': {
      'packages': [
          {
              'package': Var('fuchsia_sdk_cipd_prefix') + '${{platform}}',
              'version': Var('fuchsia_version'),
          },
      ],
      'condition': 'checkout_fuchsia_no_hooks',
      'dep_type': 'cipd',
  },
  'third_party/google_benchmark_chrome': {
    'url': Var('chromium_url') + '/chromium/src/third_party/google_benchmark.git' + '@' + '57f4041d982b6900be1f525cfe909212a14d95c6',
  },
  'third_party/google_benchmark_chrome/src': {
    'url': Var('chromium_url') + '/external/github.com/google/benchmark.git' + '@' + 'a6b4a9f00b36c19af9997d68a1cf6e0b5b3d032c',
  },
  'third_party/fuzztest':
    Var('chromium_url') + '/chromium/src/third_party/fuzztest.git' + '@' + '346bb3fce46bd9505a6b11cfe6b3c3042723bd1a',
  'third_party/fuzztest/src':
    Var('chromium_url') + '/external/github.com/google/fuzztest.git' + '@' + '5f4474e9d9286e9cf40e309979abe19069d8acde',
  'third_party/googletest/src':
    Var('chromium_url') + '/external/github.com/google/googletest.git' + '@' + 'a46a6012a133c7c0929fdae55b321c2fe65f3c5f',
  'third_party/highway/src':
    Var('chromium_url') + '/external/github.com/google/highway.git' + '@' + '2607d3b5b0113992fe84d3848859eae13b3b52c1',
  'third_party/icu':
    Var('chromium_url') + '/chromium/deps/icu.git' + '@' + 'd3c612f4414600bd06a68db89944df130b5bc248',
  'third_party/instrumented_libs': {
    'url': Var('chromium_url') + '/chromium/third_party/instrumented_libraries.git' + '@' + '490c778c3a20405a7bfbe42bc426e5d18e5b3bab',
    'condition': 'checkout_instrumented_libraries',
  },
  'third_party/ittapi': {
    # Force checkout ittapi libraries to pass v8 header includes check on
    # bots that has check_v8_header_includes enabled.
    'url': Var('chromium_url') + '/external/github.com/intel/ittapi' + '@' + 'a3911fff01a775023a06af8754f9ec1e5977dd97',
    'condition': "checkout_ittapi or check_v8_header_includes",
  },
  'third_party/jinja2':
    Var('chromium_url') + '/chromium/src/third_party/jinja2.git' + '@' + 'fde3c16799bd5937e4f15aedc9037c89f96236b9',
  'third_party/jsoncpp/src':
    Var('chromium_url') + '/external/github.com/open-source-parsers/jsoncpp.git'+ '@' + '93acc7b155048f1a66c43235cc205519f1e7a6ea',
  'third_party/libc++/src':
    Var('chromium_url') + '/external/github.com/llvm/llvm-project/libcxx.git' + '@' + '2c7b4048084397454ff423895c23c40a86dbf214',
  'third_party/libc++abi/src':
    Var('chromium_url') + '/external/github.com/llvm/llvm-project/libcxxabi.git' + '@' + '8e42446dd61148a66220123297964bd3856e7cf9',
  'third_party/libpfm4':
    Var('chromium_url') + '/chromium/src/third_party/libpfm4.git' + '@' + 'e02968592ac424c7f45441249e7175d4549a5e3a',
  'third_party/libpfm4/src':
    Var('chromium_url') + '/external/git.code.sf.net/p/perfmon2/libpfm4.git' + '@' + '186a658c3e1e49b0ee3da414a7db8ba971a00cb5',
  'third_party/libunwind/src':
    Var('chromium_url') + '/external/github.com/llvm/llvm-project/libunwind.git' + '@' + '9e44ea2ff4f53a60c2a3d2b6f511d7bab9ab12f0',
  'third_party/llvm-libc/src':
    Var('chromium_url') + '/external/github.com/llvm/llvm-project/libc.git' + '@' + '7d33739396cb4cfdebcbc02958c8fa133411d69f',
  'third_party/llvm-build/Release+Asserts': {
    'dep_type': 'gcs',
    'bucket': 'chromium-browser-clang',
    'objects': [
      {
        'object_name': 'Linux_x64/clang-android-runtime-library-llvmorg-24-init-17044-gc3223914-47.tar.xz',
        'sha256sum': 'fce4d3a83109855ae9a84f1a6f028d044b6769800cdba1664cbddd34409e24c0',
        'size_bytes': 6182336,
        'generation': 1794868672418210,
        'condition': 'checkout_android',
      },
      {
        'object_name': 'Linux_x64/clang-llvmorg-24-init-17044-gc3223914-47.tar.xz',
        'sha256sum': '5b48b726cd92de35ee50104ec8cd8ff190af50be8e7349c547b36a1852df5aa5',
        'size_bytes': 58978608,
        'generation': 1794868669800634,
        'condition': 'host_os == "linux"',
      },
      {
        'object_name': 'Linux_x64/clang-tidy-llvmorg-24-init-17044-gc3223914-47.tar.xz',
        'sha256sum': 'f6f5e3dd5b80d85ae8246b16942a4244ecf35fb220e65e55cb9231a71805c686',
        'size_bytes': 15225284,
        'generation': 1794868669795383,
        'condition': 'host_os == "linux" and checkout_clang_tidy',
      },
      {
        'object_name': 'Linux_x64/clangd-llvmorg-24-init-17044-gc3223914-47.tar.xz',
        'sha256sum': 'edf1192097f96cd2873d368c3c34db3ad6c7ac3799c9e7f9f0cef2872b5323eb',
        'size_bytes': 15587028,
        'generation': 1794868669788296,
        'condition': 'host_os == "linux" and checkout_clangd',
      },
      {
        'object_name': 'Linux_x64/llvm-code-coverage-llvmorg-24-init-17044-gc3223914-47.tar.xz',
        'sha256sum': '6d4470bb1f9455db226aa59c520cd090ffc2eafa2356efc129ac8b55937a9c7a',
        'size_bytes': 2436264,
        'generation': 1794868670066824,
        'condition': 'host_os == "linux" and checkout_clang_coverage_tools',
      },
      {
        'object_name': 'Linux_x64/llvmobjdump-llvmorg-24-init-17044-gc3223914-47.tar.xz',
        'sha256sum': 'b8a3f5abb0272cf7c1804fc560c3e92ff194179b054d8cfc0a21be4ddcc3342f',
        'size_bytes': 6031520,
        'generation': 1794868669868478,
        'condition': '(checkout_linux or checkout_mac or checkout_android) and host_os == "linux"',
      },
      {
        'object_name': 'Mac/clang-llvmorg-24-init-17044-gc3223914-47.tar.xz',
        'sha256sum': '3627c11836b6b523740e60c433876e96c4f36da2d46c7889bd6eaf3aa32b840e',
        'size_bytes': 55483392,
        'generation': 1794868679526500,
        'condition': 'host_os == "mac" and host_cpu == "x64"',
      },
      {
        'object_name': 'Mac/clang-mac-runtime-library-llvmorg-24-init-17044-gc3223914-47.tar.xz',
        'sha256sum': '2d61eee9874eb1b834a9ba9c8f1d09d2b8d927bbbc739cb34a5168d974b1262f',
        'size_bytes': 1285408,
        'generation': 1794868686879433,
        'condition': 'checkout_mac and not host_os == "mac"',
      },
      {
        'object_name': 'Mac/clang-tidy-llvmorg-24-init-17044-gc3223914-47.tar.xz',
        'sha256sum': '4fdc070d5d2efbe56767583eaa06e1913bb9e9e74b6ea1baa0f7da2cac16439a',
        'size_bytes': 14475760,
        'generation': 1794868679574575,
        'condition': 'host_os == "mac" and host_cpu == "x64" and checkout_clang_tidy',
      },
      {
        'object_name': 'Mac/clangd-llvmorg-24-init-17044-gc3223914-47.tar.xz',
        'sha256sum': '78383abed3bb6640ec1d8f5969644aa596fb701eea3f4943e330404d069112dc',
        'size_bytes': 15583452,
        'generation': 1794868679585261,
        'condition': 'host_os == "mac" and host_cpu == "x64" and checkout_clangd',
      },
      {
        'object_name': 'Mac/llvm-code-coverage-llvmorg-24-init-17044-gc3223914-47.tar.xz',
        'sha256sum': '02abeb1c38cdd40ccb7eb5f8aa58590dad8d71fdfc6e83ada7984be050659864',
        'size_bytes': 2315384,
        'generation': 1794868679768658,
        'condition': 'host_os == "mac" and host_cpu == "x64" and checkout_clang_coverage_tools',
      },
      {
        'object_name': 'Mac/llvmobjdump-llvmorg-24-init-17044-gc3223914-47.tar.xz',
        'sha256sum': '6724214965067327b7ad58def5c586bc68b257d4bae9e142937e046d20d71061',
        'size_bytes': 5597208,
        'generation': 1794868679644760,
        'condition': 'host_os == "mac" and host_cpu == "x64"',
      },
      {
        'object_name': 'Mac_arm64/clang-llvmorg-24-init-17044-gc3223914-47.tar.xz',
        'sha256sum': '7b205d9d319c8413b345b4bfd8124cdeb9eda04b065432d62120d87a8ab375a2',
        'size_bytes': 45561260,
        'generation': 1794868688710861,
        'condition': 'host_os == "mac" and host_cpu == "arm64"',
      },
      {
        'object_name': 'Mac_arm64/clang-tidy-llvmorg-24-init-17044-gc3223914-47.tar.xz',
        'sha256sum': 'd4073a6b7469fb08ef477ec47cac7e18c4b6112e7e3aaf2378a9d1735241e615',
        'size_bytes': 12350028,
        'generation': 1794868688869344,
        'condition': 'host_os == "mac" and host_cpu == "arm64" and checkout_clang_tidy',
      },
      {
        'object_name': 'Mac_arm64/clangd-llvmorg-24-init-17044-gc3223914-47.tar.xz',
        'sha256sum': 'cd34998cf2ed0fed74de6718904610748bfc17cf1b934b83304022d9f0dd86dd',
        'size_bytes': 12339776,
        'generation': 1794868688847032,
        'condition': 'host_os == "mac" and host_cpu == "arm64" and checkout_clangd',
      },
      {
        'object_name': 'Mac_arm64/llvm-code-coverage-llvmorg-24-init-17044-gc3223914-47.tar.xz',
        'sha256sum': '515366cf2a0a47e8c64fe3b895af8e2bc55af4f7fc3a2d2fb2ed9704191cc553',
        'size_bytes': 1902824,
        'generation': 1794868689035742,
        'condition': 'host_os == "mac" and host_cpu == "arm64" and checkout_clang_coverage_tools',
      },
      {
        'object_name': 'Mac_arm64/llvmobjdump-llvmorg-24-init-17044-gc3223914-47.tar.xz',
        'sha256sum': '3959994bbaa2dab47530fbdf41be165305d65b912789813170c9b1005cfb5df1',
        'size_bytes': 5275476,
        'generation': 1794868688825744,
        'condition': 'host_os == "mac" and host_cpu == "arm64"',
      },
      {
        'object_name': 'Win/clang-llvmorg-24-init-17044-gc3223914-47.tar.xz',
        'sha256sum': '94935937927b9b4b9d5c8a4c0337d63f6d9d29a8644814392397304f71b87743',
        'size_bytes': 53264572,
        'generation': 1794868698418885,
        'condition': 'host_os == "win"',
      },
      {
        'object_name': 'Win/clang-tidy-llvmorg-24-init-17044-gc3223914-47.tar.xz',
        'sha256sum': 'f041c919f8f87b28b6e60d42e0727aab0932737a6d3628a9aa2a2d4f159e8966',
        'size_bytes': 15517860,
        'generation': 1794868698516842,
        'condition': 'host_os == "win" and checkout_clang_tidy',
      },
      {
        'object_name': 'Win/clang-win-runtime-library-llvmorg-24-init-17044-gc3223914-47.tar.xz',
        'sha256sum': '99ac582fe2c15dc7298476b55941ca106763d246848f53ad400de78119113f17',
        'size_bytes': 2713444,
        'generation': 1794868705983753,
        'condition': 'checkout_win and not host_os == "win"',
      },
      {
        'object_name': 'Win/clangd-llvmorg-24-init-17044-gc3223914-47.tar.xz',
        'sha256sum': '3093126ce4bb22b228ea05ca2b36cd5e2438a88a9192d3c47a247dfa624e3a11',
        'size_bytes': 15974888,
        'generation': 1794868698507495,
       'condition': 'host_os == "win" and checkout_clangd',
      },
      {
        'object_name': 'Win/llvm-code-coverage-llvmorg-24-init-17044-gc3223914-47.tar.xz',
        'sha256sum': '5315c82568debd2ea0e7543e8fb6cfcef03ae6ab1a51b2e52e53227a878cb8c6',
        'size_bytes': 2617336,
        'generation': 1794868698761423,
        'condition': 'host_os == "win" and checkout_clang_coverage_tools',
      },
      {
        'object_name': 'Win/llvmobjdump-llvmorg-24-init-17044-gc3223914-47.tar.xz',
        'sha256sum': '90b3cff793fdbb563fe5b59cea2f460f27242035630f13906c153c5c906224a3',
        'size_bytes': 6135016,
        'generation': 1794868698629785,
        'condition': '(checkout_linux or checkout_mac or checkout_android) and host_os == "win"',
      },
    ],
  },
  'third_party/logdog/logdog':
    Var('chromium_url') + '/infra/luci/luci-py/client/libs/logdog' + '@' + 'dd1f72c4fefbd4a1fd3b6bdb5d56afede3612347',
  'third_party/markupsafe':
    Var('chromium_url') + '/chromium/src/third_party/markupsafe.git' + '@' + '4256084ae14175d38a3ff7d739dca83ae49ccec6',
  'third_party/ninja': {
    'packages': [
      {
        'package': 'infra/3pp/tools/ninja/${{platform}}',
        'version': Var('ninja_version'),
      }
    ],
    'dep_type': 'cipd',
    'condition': 'host_cpu != "s390x" and host_os != "zos" and host_cpu != "ppc64"'
  },
  'third_party/partition_alloc': {
    'url': Var('chromium_url') + '/chromium/src/base/allocator/partition_allocator.git@' + Var('partition_alloc_version'),
    'condition': 'not build_with_chromium',
  },
  'third_party/perfetto':
    Var('chromium_url') + '/external/github.com/google/perfetto.git' + '@' + 'd348e36fa3d9db11b0ef52b9f8127a553832fb6e',
  'third_party/protobuf':
    Var('chromium_url') + '/chromium/src/third_party/protobuf.git' + '@' + '3813fd5b3029807583e5d2549a40d78fa499c3e8',
  'third_party/re2/src':
    Var('chromium_url') + '/external/github.com/google/re2.git' + '@' + '2da0056814cf180480a19f5cf811e7e1c054bf6d',
  'third_party/requests': {
      'url': Var('chromium_url') + '/external/github.com/kennethreitz/requests.git' + '@' + 'c2b307dbefe21177af03f9feb37181a89a799fcc',
      'condition': 'checkout_android',
  },
  'tools/rust':
    Var('chromium_url') + '/chromium/src/tools/rust' + '@' + 'a651c85f8b2b70e6b33b529bb293b52709730156',
  'tools/win':
    Var('chromium_url') + '/chromium/src/tools/win' + '@' + 'd72db9e3040567a7589e00c0ad7ddf25c3b070bc',
  'third_party/rust':
    Var('chromium_url') + '/chromium/src/third_party/rust' + '@' + '7661f9bcc77d614626b818bc40bd8398e4a48d9c',
  'third_party/rust-toolchain': {
    'dep_type': 'gcs',
    'bucket': 'chromium-browser-clang',
    'objects': [
      {
        'object_name': 'Linux_x64/rust-toolchain-96a18bf5ffa2b24f9cf3d1c731a9eb121c1acad9-5-llvmorg-24-init-17044-gc3223914.tar.xz',
        'sha256sum': '73e395668def3e0a488f5edffd3ccaeb86a99f0476ed06c04930c6895094b212',
        'size_bytes': 287866428,
        'generation': 1794385348294495,
        'condition': 'host_os == "linux"',
      },
      {
        'object_name': 'Mac/rust-toolchain-96a18bf5ffa2b24f9cf3d1c731a9eb121c1acad9-5-llvmorg-24-init-17044-gc3223914.tar.xz',
        'sha256sum': '0888daf1c1215ac4556028d7e6df7d5fa0c57336bff64c570a57f93fef0da565',
        'size_bytes': 275787508,
        'generation': 1794385352157176,
        'condition': 'host_os == "mac" and host_cpu == "x64"',
      },
      {
        'object_name': 'Mac_arm64/rust-toolchain-96a18bf5ffa2b24f9cf3d1c731a9eb121c1acad9-5-llvmorg-24-init-17044-gc3223914.tar.xz',
        'sha256sum': '64a506650c18e57eeae11d493c41faf3a712885b7b65d993f4c76eac7f0498b7',
        'size_bytes': 259550672,
        'generation': 1794385356196372,
        'condition': 'host_os == "mac" and host_cpu == "arm64"',
      },
      {
        'object_name': 'Win/rust-toolchain-96a18bf5ffa2b24f9cf3d1c731a9eb121c1acad9-5-llvmorg-24-init-17044-gc3223914.tar.xz',
        'sha256sum': 'c3c1f2e465ed50a67e2ba5c5579c3df46e2fb445ccb73d063869087972d84e1c',
        'size_bytes': 427637256,
        'generation': 1794385360031394,
        'condition': 'host_os == "win"',
      },
    ],
  },
  # libclang + Python bindings, for the tools/metagen harvest. Built from
  # the same LLVM revision as the clang and rust packages, so it stays in
  # step with the toolchain metagen borrows its resource-dir from; bump
  # this pin along with the clang/rust roll.
  #
  # The GCS object is named rust-libclang-*, but the entry uses the name
  # Chromium gives it (src/third_party/llvm-libclang, maintained there by
  # tools/clang/scripts/sync_deps.py) so //third_party/llvm-libclang
  # resolves in both trees.
  'third_party/llvm-libclang': {
    'dep_type': 'gcs',
    'bucket': 'chromium-browser-clang',
    'objects': [
      {
        'object_name': 'Linux_x64/rust-libclang-96a18bf5ffa2b24f9cf3d1c731a9eb121c1acad9-5-llvmorg-24-init-17044-gc3223914.tar.xz',
        'sha256sum': 'bdfa70274badca548d73b95f353ebc6451c96c6ada1e561c6da880f857f21bb6',
        'size_bytes': 23186936,
        'generation': 1794385350228067,
        'condition': 'host_os == "linux"',
      },
      {
        'object_name': 'Mac/rust-libclang-96a18bf5ffa2b24f9cf3d1c731a9eb121c1acad9-5-llvmorg-24-init-17044-gc3223914.tar.xz',
        'sha256sum': 'daa332f90f0237bd1389cfa482957eee5f08cd341c42b3c0f7ec9ee96cf90544',
        'size_bytes': 23333728,
        'generation': 1794385354028288,
        'condition': 'host_os == "mac" and host_cpu == "x64"',
      },
      {
        'object_name': 'Mac_arm64/rust-libclang-96a18bf5ffa2b24f9cf3d1c731a9eb121c1acad9-5-llvmorg-24-init-17044-gc3223914.tar.xz',
        'sha256sum': '4bfdfe46686aa3c1f8f6fd9447fac5e37e699bccf213138b279c99a71c732a03',
        'size_bytes': 20988176,
        'generation': 1794385358033896,
        'condition': 'host_os == "mac" and host_cpu == "arm64"',
      },
      {
        'object_name': 'Win/rust-libclang-96a18bf5ffa2b24f9cf3d1c731a9eb121c1acad9-5-llvmorg-24-init-17044-gc3223914.tar.xz',
        'sha256sum': '7b4dd46967b0a84b83ab6aa6f3999ca18b15926c5f05a481445e47aaed2558e2',
        'size_bytes': 21906524,
        'generation': 1794385361865408,
        'condition': 'host_os == "win"',
      },
    ],
  },
  'third_party/siso/cipd': {
    'packages': [
      {
        'package': 'build/siso/${{platform}}',
        'version': Var('siso_version'),
      }
    ],
    'dep_type': 'cipd',
    'condition': 'not build_with_chromium and host_cpu != "s390x" and host_os != "zos" and host_cpu != "ppc64"',
  },
  # Always download Linux x64 package regardless of host OS for RBE workers.
  'third_party/cpython3/linux-amd64': {
    'packages': [
      {
        'package': 'infra/3pp/tools/cpython3/linux-amd64',
        'version': Var('cpython3_version'),
      }
    ],
    'condition': 'not build_with_chromium and host_cpu != "s390x" and host_os != "zos" and host_cpu != "ppc64"',
    'dep_type': 'cipd',
  },
  # Host platform package.
  'third_party/cpython3/host': {
    'packages': [
      {
        'package': 'infra/3pp/tools/cpython3/${{platform}}',
        'version': Var('cpython3_version'),
      }
    ],
    'condition': 'not build_with_chromium and host_cpu != "s390x" and host_os != "zos" and host_cpu != "ppc64"',
    'dep_type': 'cipd',
  },
  'third_party/zlib':
    Var('chromium_url') + '/chromium/src/third_party/zlib.git'+ '@' + '401d1853956d408bfd327ee5e54c1c7b67086f87',
  'tools/clang':
    Var('chromium_url') + '/chromium/src/tools/clang.git' + '@' + '2c77491953fc35d6bd23b8ff72defc692f91ec52',
  'tools/protoc_wrapper':
    Var('chromium_url') + '/chromium/src/tools/protoc_wrapper.git' + '@' + '5c77c0c9fdebb2d87378bcea183e4c6937fd8c53',
  'tools/python': {
    'url': Var('chromium_url') + '/chromium/src/tools/python.git' + '@' + '54763b0a68ecca4842b8ef32793b82811337a67b',
    'condition': 'checkout_android',
  },
  'third_party/abseil-cpp': {
    'url': Var('chromium_url') + '/chromium/src/third_party/abseil-cpp.git' + '@' + '728ff0c29db223cb9a4394e8f113c01fdbb52805',
    'condition': 'not build_with_chromium',
  },
  'third_party/boringssl/src': {
    'url': Var('boringssl_url') + '/boringssl.git' + '@' + 'ebd922adc708a48b0cccfcdf81e854a3fdc1b7b6',
    'condition': 'not build_with_chromium',
  },
  'third_party/nasm': {
    'url': Var('chromium_url') + '/chromium/deps/nasm.git' + '@' + '54b1dcf7995006337c54f6862fd6f77a01e23e4e',
    'condition': 'not build_with_chromium',
  },
  'third_party/fadec/src': {
    'url': Var('chromium_url') + '/external/github.com/aengelke/fadec.git' + '@' + 'c9f78f532b9004de278489019ab2c6c28ae9746e',
  },
  'third_party/disarm/src': {
    'url': Var('chromium_url') + '/external/github.com/aengelke/disarm.git' + '@' + '8770f6d6f0c62e7e008f4bf0e04d22197ecb98e3',
  },
  'third_party/zoslib': {
    'url': Var('chromium_url') + '/external/github.com/ibmruntimes/zoslib.git' + '@' + '804b13554e8dd6972a591c1d6e532514fadd42a8',
    'condition': 'host_os == "zos"',
  }
}

include_rules = [
  # Everybody can use some things.
  '+include',
  '+unicode',
  '+third_party/dragonbox/src/include',
  '+third_party/fast_float/src/include',
  '+third_party/fdlibm',
  '+third_party/fp16/src/include',
  '+third_party/fuzztest',
  '+third_party/ittapi/include',
  '+third_party/v8/codegen',
  '+third_party/vtune',
  '+hwy/highway.h',
  '+openssl',
  '+simdutf.h',
  # Abseil features are allow-listed. Please use your best judgement when adding
  # to this set -- if in doubt, email v8-dev@. For general guidance, refer to
  # the Chromium guidelines (though note that some requirements in V8 may be
  # different to Chromium's):
  # https://chromium.googlesource.com/chromium/src/+/main/styleguide/c++/c++-features.md
  '+absl/container/flat_hash_map.h',
  '+absl/container/flat_hash_set.h',
  '+absl/container/btree_map.h',
  '+absl/functional/overload.h',
  '+absl/numeric/int128.h',
  '+absl/status',
  '+absl/strings/str_format.h',
  '+absl/synchronization/mutex.h',
  '+absl/time/time.h',
  # Some abseil features are explicitly banned.
  '-absl/types/any.h', # Requires RTTI.
  '-absl/types/flags', # Requires RTTI.
  '-absl/functional/function_ref.h', # Use base::FunctionRef
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
        'python3',
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
        'python3',
        'build/landmines.py',
        '--landmine-scripts',
        'tools/get_landmines.py',
    ],
  },
  {
    'name': 'bazel',
    'pattern': '.',
    'condition': 'download_prebuilt_bazel',
    'action': [ 'python3',
                'third_party/depot_tools/download_from_google_storage.py',
                '--bucket', 'chromium-v8-prebuilt-bazel/linux',
                '--no_resume',
                '-s', 'tools/bazel/bazel.sha1',
                '--platform=linux*',
    ],
  },
  # Pull dsymutil binaries using checked-in hashes.
  {
    'name': 'dsymutil_mac_arm64',
    'pattern': '.',
    'condition': 'host_os == "mac" and host_cpu == "arm64"',
    'action': [ 'python3',
                'third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--bucket', 'chromium-browser-clang',
                '-s', 'tools/clang/dsymutil/bin/dsymutil.arm64.sha1',
                '-o', 'tools/clang/dsymutil/bin/dsymutil',
    ],
  },
  {
    'name': 'dsymutil_mac_x64',
    'pattern': '.',
    'condition': 'host_os == "mac" and host_cpu == "x64"',
    'action': [ 'python3',
                'third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--bucket', 'chromium-browser-clang',
                '-s', 'tools/clang/dsymutil/bin/dsymutil.x64.sha1',
                '-o', 'tools/clang/dsymutil/bin/dsymutil',
    ],
  },
  {
    'name': 'gcmole',
    'pattern': '.',
    'condition': 'download_gcmole',
    'action': [ 'python3',
                'third_party/depot_tools/download_from_google_storage.py',
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
    'action': [ 'python3',
                'third_party/depot_tools/download_from_google_storage.py',
                '--bucket', 'chrome-v8-jsfunfuzz',
                '-u', '--no_resume',
                '-s', 'tools/jsfunfuzz/jsfunfuzz.tar.gz.sha1',
                '--platform=linux*',
    ],
  },
  {
    'name': 'llvm_symbolizer',
    'pattern': '.',
    'condition': 'download_prebuilt_arm64_llvm_symbolizer',
    'action': [ 'python3',
                'third_party/depot_tools/download_from_google_storage.py',
                '--bucket', 'chromium-v8/llvm/arm64',
                '--no_resume',
                '-s', 'tools/sanitizers/linux/arm64/llvm-symbolizer.sha1',
                '--platform=linux*',
    ],
  },
  {
    'name': 'wasm_spec_tests',
    'pattern': '.',
    'action': [ 'python3',
                'third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '-u',
                '--bucket', 'v8-wasm-spec-tests',
                '-s', 'test/wasm-spec-tests/tests.tar.gz.sha1',
    ],
  },
  {
    'name': 'wasm_js',
    'pattern': '.',
    'action': [ 'python3',
                'third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '-u',
                '--bucket', 'v8-wasm-spec-tests',
                '-s', 'test/wasm-js/tests.tar.gz.sha1',
    ],
  },
  {
    # Case-insensitivity for the Win SDK. Must run before win_toolchain below.
    'name': 'ciopfs_linux',
    'pattern': '.',
    'condition': 'checkout_win and host_os == "linux"',
    'action': [ 'python3',
                'third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--bucket', 'chromium-browser-clang/ciopfs',
                '-s', 'build/ciopfs.sha1',
    ]
  },
  {
    # Update the Windows toolchain if necessary.
    'name': 'win_toolchain',
    'pattern': '.',
    'condition': 'checkout_win',
    'action': ['python3', 'build/vs_toolchain.py', 'update', '--force'],
  },
  {
    # Update the Mac toolchain if necessary.
    'name': 'mac_toolchain',
    'pattern': '.',
    'condition': 'checkout_mac',
    'action': ['python3', 'build/mac_toolchain.py'],
  },
  {
    # Update LASTCHANGE.
    'name': 'lastchange',
    'pattern': '.',
    'action': ['python3', 'build/util/lastchange.py',
               '-o', 'build/util/LASTCHANGE'],
  },
  {
    'name': 'Download Fuchsia SDK from GCS',
    'pattern': '.',
    'condition': 'checkout_fuchsia',
    'action': [
      'python3',
      'build/fuchsia/update_sdk.py',
      '--cipd-prefix={fuchsia_sdk_cipd_prefix}',
      '--version={fuchsia_version}',
    ],
  },
  {
    'name': 'Download Fuchsia system images',
    'pattern': '.',
    'condition': 'checkout_fuchsia and checkout_fuchsia_product_bundles',
    'action': [
      'python3',
      'build/fuchsia/update_product_bundles.py',
      '{checkout_fuchsia_boot_images}',
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
  {
    'name': 'vpython3_common',
    'pattern': '.',
    'action': [ 'vpython3',
                '-vpython-spec', 'vpython.toml',
                '-vpython-tool', 'install',
    ],
  },
  {
    'name': 'check_v8_header_includes',
    'pattern': '.',
    'condition': 'check_v8_header_includes',
    'action': [
      'python3',
      'tools/generate-header-include-checks.py',
    ],
  },
  {
    'name': 'checkout_v8_builtins_pgo_profiles',
    'pattern': '.',
    'condition': 'checkout_v8_builtins_pgo_profiles',
    'action': [
      'python3',
      'tools/builtins-pgo/download_profiles.py',
      'download',
      '--quiet',
    ],
  },
  # Configure remote exec cfg files
  {
    'name': 'download_and_configure_reclient_cfgs',
    'pattern': '.',
    'condition': 'download_remoteexec_cfg and not build_with_chromium',
    'action': ['python3',
               'buildtools/reclient_cfgs/configure_reclient_cfgs.py',
               '--rbe_instance',
               Var('rbe_instance'),
               '--rewrapper_cfg_project',
               Var('rewrapper_cfg_project'),
               '--quiet',
               ],
  },
  {
    'name': 'configure_reclient_cfgs',
    'pattern': '.',
    'condition': 'not download_remoteexec_cfg and not build_with_chromium',
    'action': ['python3',
               'buildtools/reclient_cfgs/configure_reclient_cfgs.py',
               '--rbe_instance',
               Var('rbe_instance'),
               '--rewrapper_cfg_project',
               Var('rewrapper_cfg_project'),
               '--skip_remoteexec_cfg_fetch',
               ],
  },
  # Configure Siso for developer builds.
  {
    'name': 'configure_siso',
    'pattern': '.',
    'condition': 'not build_with_chromium',
    'action': ['python3',
               'build/config/siso/configure_siso.py',
               '--rbe_instance',
               Var('rbe_instance'),
               ],
  },
]

recursedeps = [
  'build',
  'buildtools',
  'third_party/instrumented_libs',
  'third_party/v8-perf',
]
