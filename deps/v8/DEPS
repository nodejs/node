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
  'android_ndk_version': Str('2@30.0.15729638'),

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

  # reclient CIPD package version
  'reclient_version': 're_client_version:0.185.0.db415f21-gomaip',

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
  'gn_version': 'git_revision:a99d46a9d04c770d6bb87387058e1d7b151758ce',

  # ninja CIPD package version
  # https://chrome-infra-packages.appspot.com/p/infra/3pp/tools/ninja
  'ninja_version': 'version:3@1.12.1.chromium.4',

  # siso CIPD package version
  'siso_version': 'git_revision:efbbe7f1892211b5e9512576843a3c247b6a6d7c',

  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling Fuchsia sdk
  # and whatever else without interference from each other.
  'fuchsia_version': 'version:33.20260824.1.1',

  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling partition_alloc_version
  # and whatever else without interference from each other.
  'partition_alloc_version': 'c029851c21b7e1154fa3964e08c97710adc92cc7',

  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling android_sdk_build-tools_version
  # and whatever else without interference from each other.
  'android_sdk_build-tools_version': 'febJrTgiK9s1ANoUlc4Orn3--zs9GjGCj2vQc8g7OaMC',
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
  'android_sdk_platforms_version': 'WhtP32Q46ZHdTmgCgdauM3ws_H9iPoGKEZ_cPggcQ6wC',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling android_sdk_tools-lint_version
  # and whatever else without interference from each other.
  'android_sdk_cmdline-tools_version': 'gekOVsZjseS1w9BXAT3FsoW__ByGDJYS9DgqesiwKYoC',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling jetstream_3.0-custom_revision
  # and whatever else without interference from each other.
  'jetstream_3.0-custom_revision': 'ffde597c189c9c781986a6247a5844b1c04e4a8a',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling agents-internal
  # and whatever else without interference from each other.
  'agents_internal_revision': 'd2d5adf5a243d90aba25902a46fc32e7a4f5ab99',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling agents-public
  # and whatever else without interference from each other.
  'agents_public_revision': '73470db4a7810560ec92e8aa27ff3c6a8a36bea5',
}

deps = {
  'agents/shared': {
    'url': Var('chromium_url') + '/chromium/agents.git' + '@' + Var('agents_public_revision'),
  },
  'agents/internal': {
    'url': Var('chrome_internal_url') + '/chrome/agents-internal.git' + '@' + Var('agents_internal_revision'),
    'condition': 'checkout_agents_internal',
  },
  'build':
    Var('chromium_url') + '/chromium/src/build.git' + '@' + '8ca8a372cab9ff143072204053d30fbc2c17bd7a',
  'buildtools':
    Var('chromium_url') + '/chromium/src/buildtools.git' + '@' + '6f6a5dbf04b734214f3b1f386567d101ec9d607e',
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
  'buildtools/reclient': {
    'packages': [
      {
        'package': 'infra/rbe/client/${{platform}}',
        'version': Var('reclient_version'),
      }
    ],
    'dep_type': 'cipd',
    'condition': '(host_os == "linux" or host_os == "mac" or host_os == "win") and host_cpu != "s390x" and host_os != "zos" and host_cpu != "ppc64" and (host_cpu != "arm64" or host_os == "mac")',
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
    Var('chromium_url') + '/external/github.com/tc39/test262.git' + '@' + '419d3e0a2273ba01a3bfcbec423f2801425b8e93',
  'third_party/android_platform': {
    'url': Var('chromium_url') + '/chromium/src/third_party/android_platform.git' + '@' + 'e3919359f2387399042d31401817db4a02d756ec',
    'condition': 'checkout_android',
  },
  'third_party/android_sdk/public': {
      'packages': [
          {
              'package': 'chromium/third_party/android_sdk/public/build-tools/37.0.0',
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
        'package': 'chromium/third_party/android_toolchain/android_toolchain',
        'version': 'version:' + Var('android_ndk_version'),
      },
    ],
    'condition': 'checkout_android',
    'dep_type': 'cipd',
  },
  'third_party/catapult': {
    'url': Var('chromium_url') + '/catapult.git' + '@' + '50c971fc0958d8c1fb0adcc97f41b90042fa7c87',
    'condition': 'checkout_android',
  },
  'third_party/clang-format/script':
    Var('chromium_url') + '/external/github.com/llvm/llvm-project/clang/tools/clang-format.git' + '@' + '70510081984cfcdb14a15b3e08dfe9776dc7ed37',
  'third_party/colorama/src': {
    'url': Var('chromium_url') + '/external/colorama.git' + '@' + '3de9f013df4b470069d03d250224062e8cf15c49',
    'condition': 'checkout_android',
  },
  'third_party/cpu_features': {
    'url': Var('chromium_url') + '/chromium/src/third_party/cpu_features.git' + '@' + '4974e2063946d9dd2a417c27db139fd982ff447a',
    'condition': 'checkout_android',
  },
  'third_party/cpu_features/src': {
    'url': Var('chromium_url') + '/external/github.com/google/cpu_features.git' + '@' + '81d13c49649f0714dd41fb56bb246398b6584085',
    'condition': 'checkout_android',
  },
  'third_party/depot_tools':
    Var('chromium_url') + '/chromium/tools/depot_tools.git' + '@' + 'cb70c994a656601dc6a0d423f49ff57503bd70bc',
  'third_party/dragonbox/src':
    Var('chromium_url') + '/external/github.com/jk-jeon/dragonbox.git' + '@' + 'beeeef91cf6fef89a4d4ba5e95d47ca64ccb3a44',
  'third_party/fp16/src':
    Var('chromium_url') + '/external/github.com/Maratyszcza/FP16.git' + '@' + '782eea126dc5c755827be751a099eb01826175cf',
  'third_party/fast_float/src':
    Var('chromium_url') + '/external/github.com/fastfloat/fast_float.git' + '@' + '34164f547b7df3f5d794ff67e9f885c36819ebfc',
  'third_party/fuchsia-gn-sdk': {
    'url': Var('chromium_url') + '/chromium/src/third_party/fuchsia-gn-sdk.git' + '@' + '740d23bfe5b137c2c387a4b421f120f6126845de',
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
    'url': Var('chromium_url') + '/chromium/src/third_party/google_benchmark.git' + '@' + 'c3b654389bf74fac1f2d926d0439506f06c66751',
  },
  'third_party/google_benchmark_chrome/src': {
    'url': Var('chromium_url') + '/external/github.com/google/benchmark.git' + '@' + '8abf1e701fbd88c8170f48fe0558247e2e5f8e7d',
  },
  'third_party/fuzztest':
    Var('chromium_url') + '/chromium/src/third_party/fuzztest.git' + '@' + 'f725b648ad0102c99f0ac367502c2bcef1d9d48f',
  'third_party/fuzztest/src':
    Var('chromium_url') + '/external/github.com/google/fuzztest.git' + '@' + '25f6bff894a558fd6b7076a22f9e881d807304e6',
  'third_party/googletest/src':
    Var('chromium_url') + '/external/github.com/google/googletest.git' + '@' + '4fe3307fb2d9f86d19777c7eb0e4809e9694dde7',
  'third_party/highway/src':
    Var('chromium_url') + '/external/github.com/google/highway.git' + '@' + '2607d3b5b0113992fe84d3848859eae13b3b52c1',
  'third_party/icu':
    Var('chromium_url') + '/chromium/deps/icu.git' + '@' + '6ebb40c594776cc2c21ea14df85a2a89a328b364',
  'third_party/instrumented_libs': {
    'url': Var('chromium_url') + '/chromium/third_party/instrumented_libraries.git' + '@' + 'd15c278eed5d38d9acf2d8054cf37baba93cef8e',
    'condition': 'checkout_instrumented_libraries',
  },
  'third_party/ittapi': {
    # Force checkout ittapi libraries to pass v8 header includes check on
    # bots that has check_v8_header_includes enabled.
    'url': Var('chromium_url') + '/external/github.com/intel/ittapi' + '@' + 'a3911fff01a775023a06af8754f9ec1e5977dd97',
    'condition': "checkout_ittapi or check_v8_header_includes",
  },
  'third_party/jinja2':
    Var('chromium_url') + '/chromium/src/third_party/jinja2.git' + '@' + 'c3027d884967773057bf74b957e3fea87e5df4d7',
  'third_party/jsoncpp/source':
    Var('chromium_url') + '/external/github.com/open-source-parsers/jsoncpp.git'+ '@' + '3347a4b86bb914cb565f0cbda19c06e109513300',
  'third_party/libc++/src':
    Var('chromium_url') + '/external/github.com/llvm/llvm-project/libcxx.git' + '@' + '97b436da4c33663581d394f4ee0a5977fc38c2f4',
  'third_party/libc++abi/src':
    Var('chromium_url') + '/external/github.com/llvm/llvm-project/libcxxabi.git' + '@' + 'fc1897a2c12aa27e703c3ed48b62eba8abf4ce19',
  'third_party/libpfm4':
    Var('chromium_url') + '/chromium/src/third_party/libpfm4.git' + '@' + '4a112befd93f8d90a9b6894b2ec4d320310e1178',
  'third_party/libpfm4/src':
    Var('chromium_url') + '/external/git.code.sf.net/p/perfmon2/libpfm4.git' + '@' + '6870a9f00412830ceaa7e4384bb92ee323e2a28f',
  'third_party/libunwind/src':
    Var('chromium_url') + '/external/github.com/llvm/llvm-project/libunwind.git' + '@' + '45120ee2331193c3650acc9c427ed267fab31d62',
  'third_party/llvm-libc/src':
    Var('chromium_url') + '/external/github.com/llvm/llvm-project/libc.git' + '@' + '43a9a99ce4b5a04954090f8a0e74bf2786f59379',
  'third_party/llvm-build/Release+Asserts': {
    'dep_type': 'gcs',
    'bucket': 'chromium-browser-clang',
    'objects': [
      {
        'object_name': 'Linux_x64/clang-android-runtime-library-llvmorg-24-init-3796-g20e97c4b-4.tar.xz',
        'sha256sum': '780597375163e78615a307a73ec66346b54c58ea3164aa6fbedf24eae85382fc',
        'size_bytes': 6210856,
        'generation': 1787607008868327,
        'condition': 'checkout_android',
      },
      {
        'object_name': 'Linux_x64/clang-llvmorg-24-init-3796-g20e97c4b-4.tar.xz',
        'sha256sum': '565f7d980b31411e76d9478731cf3f0ac907a46f274d289ea7f03d9137354053',
        'size_bytes': 56894668,
        'generation': 1787607001504676,
        'condition': 'host_os == "linux"',
      },
      {
        'object_name': 'Linux_x64/clang-tidy-llvmorg-24-init-3796-g20e97c4b-4.tar.xz',
        'sha256sum': '4290664a4fa8aa11fd1e6b2d3ff4965d26b41cca1a4950f9835ff19115c2384f',
        'size_bytes': 14877500,
        'generation': 1787607001575317,
        'condition': 'host_os == "linux" and checkout_clang_tidy',
      },
      {
        'object_name': 'Linux_x64/clangd-llvmorg-24-init-3796-g20e97c4b-4.tar.xz',
        'sha256sum': '9db50937915608c466da28ba4166cfa69a5f6bcad346233e47807f532cc3075b',
        'size_bytes': 15049952,
        'generation': 1787607001603946,
        'condition': 'host_os == "linux" and checkout_clangd',
      },
      {
        'object_name': 'Linux_x64/llvm-code-coverage-llvmorg-24-init-3796-g20e97c4b-4.tar.xz',
        'sha256sum': '34747a8e113c399daa36f75a5a0a4c0963871ceafbce7c49a3b5133f81a63bae',
        'size_bytes': 2352288,
        'generation': 1787607001865886,
        'condition': 'host_os == "linux" and checkout_clang_coverage_tools',
      },
      {
        'object_name': 'Linux_x64/llvmobjdump-llvmorg-24-init-3796-g20e97c4b-4.tar.xz',
        'sha256sum': 'cc8a621ec8cc3c48471c90cf4ed88dcc56bfb403824eda10f8b55796abaec280',
        'size_bytes': 5912568,
        'generation': 1787607001713199,
        'condition': '(checkout_linux or checkout_mac or checkout_android) and host_os == "linux"',
      },
      {
        'object_name': 'Mac/clang-llvmorg-24-init-3796-g20e97c4b-4.tar.xz',
        'sha256sum': 'ba2b707d83f7a19d81b9403b4ebe8e999d41ae35fead00a142f02b67e197fe2f',
        'size_bytes': 56420628,
        'generation': 1787607010939621,
        'condition': 'host_os == "mac" and host_cpu == "x64"',
      },
      {
        'object_name': 'Mac/clang-mac-runtime-library-llvmorg-24-init-3796-g20e97c4b-4.tar.xz',
        'sha256sum': 'da06337f54ba4b36c79e0d379557832bf82b9c7e85fe8ef78a5b953567d98de1',
        'size_bytes': 1012348,
        'generation': 1787607018227451,
        'condition': 'checkout_mac and not host_os == "mac"',
      },
      {
        'object_name': 'Mac/clang-tidy-llvmorg-24-init-3796-g20e97c4b-4.tar.xz',
        'sha256sum': 'ae5cbec1362517b6928162fddb9b160bd1c75e8fd90789718393e1cadf1342ae',
        'size_bytes': 14882308,
        'generation': 1787607010909866,
        'condition': 'host_os == "mac" and host_cpu == "x64" and checkout_clang_tidy',
      },
      {
        'object_name': 'Mac/clangd-llvmorg-24-init-3796-g20e97c4b-4.tar.xz',
        'sha256sum': '73f5de0f007f8c588593d824147963ec770b66d487a707d1c377a06e34952d89',
        'size_bytes': 16779464,
        'generation': 1787607010899287,
        'condition': 'host_os == "mac" and host_cpu == "x64" and checkout_clangd',
      },
      {
        'object_name': 'Mac/llvm-code-coverage-llvmorg-24-init-3796-g20e97c4b-4.tar.xz',
        'sha256sum': 'c47eb37873d3290cc95ba5f9027337895eeeaa199c3f70e63c5d09b710faf250',
        'size_bytes': 2411184,
        'generation': 1787607011184644,
        'condition': 'host_os == "mac" and host_cpu == "x64" and checkout_clang_coverage_tools',
      },
      {
        'object_name': 'Mac/llvmobjdump-llvmorg-24-init-3796-g20e97c4b-4.tar.xz',
        'sha256sum': '4ddefa96f6aaa04ac502610fc736528f65dc3e1b997fc4a5380081937f6ffb1a',
        'size_bytes': 5874672,
        'generation': 1787607011035157,
        'condition': 'host_os == "mac" and host_cpu == "x64"',
      },
      {
        'object_name': 'Mac_arm64/clang-llvmorg-24-init-3796-g20e97c4b-4.tar.xz',
        'sha256sum': '264d803970f9b58ddbdc90464ad82e54cef642c58ec371ba5356dfa7342ece4d',
        'size_bytes': 47226204,
        'generation': 1787607020088085,
        'condition': 'host_os == "mac" and host_cpu == "arm64"',
      },
      {
        'object_name': 'Mac_arm64/clang-tidy-llvmorg-24-init-3796-g20e97c4b-4.tar.xz',
        'sha256sum': '3e02eae391e05018fb4e95d3c90b270688a5e3d5f5b31df6c59512e602478f36',
        'size_bytes': 12960456,
        'generation': 1787607020070956,
        'condition': 'host_os == "mac" and host_cpu == "arm64" and checkout_clang_tidy',
      },
      {
        'object_name': 'Mac_arm64/clangd-llvmorg-24-init-3796-g20e97c4b-4.tar.xz',
        'sha256sum': 'a56514d09cfcb216efa297e5fecd51a2e248faf7db6edd2d444cfe78e0e219d3',
        'size_bytes': 13320572,
        'generation': 1787607020209265,
        'condition': 'host_os == "mac" and host_cpu == "arm64" and checkout_clangd',
      },
      {
        'object_name': 'Mac_arm64/llvm-code-coverage-llvmorg-24-init-3796-g20e97c4b-4.tar.xz',
        'sha256sum': 'b593c509e8f4269cf4a16272f6eddcea0f9dd47f6f3fe39b9813041d0f96d4e3',
        'size_bytes': 2031112,
        'generation': 1787607020489366,
        'condition': 'host_os == "mac" and host_cpu == "arm64" and checkout_clang_coverage_tools',
      },
      {
        'object_name': 'Mac_arm64/llvmobjdump-llvmorg-24-init-3796-g20e97c4b-4.tar.xz',
        'sha256sum': 'c67f5dba316d6367be8c153ede456fdbef0dfffad4da10aaf597ff967c07378c',
        'size_bytes': 5610096,
        'generation': 1787607020228775,
        'condition': 'host_os == "mac" and host_cpu == "arm64"',
      },
      {
        'object_name': 'Win/clang-llvmorg-24-init-3796-g20e97c4b-4.tar.xz',
        'sha256sum': '166437ede456b3d7cf0d9317287511e47714f91da547dbde02992261aeef7174',
        'size_bytes': 51467544,
        'generation': 1787607030157700,
        'condition': 'host_os == "win"',
      },
      {
        'object_name': 'Win/clang-tidy-llvmorg-24-init-3796-g20e97c4b-4.tar.xz',
        'sha256sum': '13cae70dfa207c9ec27e294f38b732b5c820ef637f48d24cc8b97f5f3a96e220',
        'size_bytes': 15134360,
        'generation': 1787607030186388,
        'condition': 'host_os == "win" and checkout_clang_tidy',
      },
      {
        'object_name': 'Win/clang-win-runtime-library-llvmorg-24-init-3796-g20e97c4b-4.tar.xz',
        'sha256sum': '4ff9a06f5fc4a711103cd890e724ae4448d67873de438542f3ac6772e68d3c63',
        'size_bytes': 2638196,
        'generation': 1787607037249082,
        'condition': 'checkout_win and not host_os == "win"',
      },
      {
        'object_name': 'Win/clangd-llvmorg-24-init-3796-g20e97c4b-4.tar.xz',
        'sha256sum': '17f1d575127724051099a778d77e609f650702248cf4e7ff3ec33b377053edd9',
        'size_bytes': 15444040,
        'generation': 1787607030301504,
       'condition': 'host_os == "win" and checkout_clangd',
      },
      {
        'object_name': 'Win/llvm-code-coverage-llvmorg-24-init-3796-g20e97c4b-4.tar.xz',
        'sha256sum': '78aa6b2b9cf057077be6705b76e43ffed83e46e17ee86da1ab699340129b1aca',
        'size_bytes': 2512968,
        'generation': 1787607030493491,
        'condition': 'host_os == "win" and checkout_clang_coverage_tools',
      },
      {
        'object_name': 'Win/llvmobjdump-llvmorg-24-init-3796-g20e97c4b-4.tar.xz',
        'sha256sum': '4b899193da883a447c75f057e3a94f5e7ab93e5df5724ad94cbd6bf82ea792b4',
        'size_bytes': 5994644,
        'generation': 1787607030276791,
        'condition': '(checkout_linux or checkout_mac or checkout_android) and host_os == "win"',
      },
    ],
  },
  'third_party/logdog/logdog':
    Var('chromium_url') + '/infra/luci/luci-py/client/libs/logdog' + '@' + '6d8ea2699c6e78b344b2fc4956322aa2c463af10',
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
    Var('chromium_url') + '/external/github.com/google/perfetto.git' + '@' + '5dbeaad4ceb91470a835426b52e0cb43a3e5ef10',
  'third_party/protobuf':
    Var('chromium_url') + '/chromium/src/third_party/protobuf.git' + '@' + '5f8c379d1fc89fe8eee16ae560dd5e514a4608da',
  'third_party/re2/src':
    Var('chromium_url') + '/external/github.com/google/re2.git' + '@' + '972a15cedd008d846f1a39b2e88ce48d7f166cbd',
  'third_party/requests': {
      'url': Var('chromium_url') + '/external/github.com/kennethreitz/requests.git' + '@' + 'c7e0fc087ceeadb8b4c84a0953a422c474093d6d',
      'condition': 'checkout_android',
  },
  'tools/rust':
    Var('chromium_url') + '/chromium/src/tools/rust' + '@' + '54d1148695dbbbfc3e00dfd05aff502acaa602e3',
  'tools/win':
    Var('chromium_url') + '/chromium/src/tools/win' + '@' + '13cb6e5d223dc49eadd082d3aef4c2a5b0e4c0a0',
  'third_party/rust':
    Var('chromium_url') + '/chromium/src/third_party/rust' + '@' + '4dec6f65bcb34c4c289736b0e6fd571b35c6c665',
  'third_party/rust-toolchain': {
    'dep_type': 'gcs',
    'bucket': 'chromium-browser-clang',
    'objects': [
      {
        'object_name': 'Linux_x64/rust-toolchain-0913b18e489ac1011b580e31fa5559654be12bfc-2-llvmorg-24-init-3796-g20e97c4b.tar.xz',
        'sha256sum': '4b131e81d157a97bcf454ad0fbb71bc2063b2a3708c858410c04201ef06134e5',
        'size_bytes': 276314860,
        'generation': 1786821088882867,
        'condition': 'host_os == "linux"',
      },
      {
        'object_name': 'Mac/rust-toolchain-0913b18e489ac1011b580e31fa5559654be12bfc-2-llvmorg-24-init-3796-g20e97c4b.tar.xz',
        'sha256sum': '12ca849195c27495073ba52ae7126d8a4a9a5807cd7a0dc6ba6b6fa612302a6d',
        'size_bytes': 264031896,
        'generation': 1786821092536805,
        'condition': 'host_os == "mac" and host_cpu == "x64"',
      },
      {
        'object_name': 'Mac_arm64/rust-toolchain-0913b18e489ac1011b580e31fa5559654be12bfc-2-llvmorg-24-init-3796-g20e97c4b.tar.xz',
        'sha256sum': 'fe128475561d7a51c797d1cb3ccea6f94fd770b167c8e0cc33bf75e7c25d8225',
        'size_bytes': 248465344,
        'generation': 1786821096222521,
        'condition': 'host_os == "mac" and host_cpu == "arm64"',
      },
      {
        'object_name': 'Win/rust-toolchain-0913b18e489ac1011b580e31fa5559654be12bfc-2-llvmorg-24-init-3796-g20e97c4b.tar.xz',
        'sha256sum': '14bc9cea5e00cb191f58204ef44d68a6794f856a76f885c50298a12d052035bc',
        'size_bytes': 414479372,
        'generation': 1786821099612317,
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
        'object_name': 'Linux_x64/rust-libclang-0913b18e489ac1011b580e31fa5559654be12bfc-2-llvmorg-24-init-3796-g20e97c4b.tar.xz',
        'sha256sum': 'a36afcfb09b2b096cd7d17b6008beb1993c3962d780a7ccc0aa4c5f544447429',
        'size_bytes': 22734560,
        'generation': 1786821090667672,
        'condition': 'host_os == "linux"',
      },
      {
        'object_name': 'Mac/rust-libclang-0913b18e489ac1011b580e31fa5559654be12bfc-2-llvmorg-24-init-3796-g20e97c4b.tar.xz',
        'sha256sum': '0ff3d2a46a0ed3851311cce6aa60f70f3d3e88e4177c2b10806c990a72461f67',
        'size_bytes': 23130796,
        'generation': 1786821094366500,
        'condition': 'host_os == "mac" and host_cpu == "x64"',
      },
      {
        'object_name': 'Mac_arm64/rust-libclang-0913b18e489ac1011b580e31fa5559654be12bfc-2-llvmorg-24-init-3796-g20e97c4b.tar.xz',
        'sha256sum': '0a95d15ec7e992b60f47b9cf98f951b32f718488aa5d15d1c8cbae81f1db6b39',
        'size_bytes': 20545284,
        'generation': 1786821097847085,
        'condition': 'host_os == "mac" and host_cpu == "arm64"',
      },
      {
        'object_name': 'Win/rust-libclang-0913b18e489ac1011b580e31fa5559654be12bfc-2-llvmorg-24-init-3796-g20e97c4b.tar.xz',
        'sha256sum': '75033b0243acf7c25227f6015c60797724b98d1de5514e9e1a374735ef76aa4e',
        'size_bytes': 21534908,
        'generation': 1786821101319840,
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
    Var('chromium_url') + '/chromium/src/third_party/zlib.git'+ '@' + '285e94b8fa95ad3b7d16b80798ec8dce6febb8c8',
  'tools/clang':
    Var('chromium_url') + '/chromium/src/tools/clang.git' + '@' + '450d823868eaee61e2b2cb986e19aec287f16d58',
  'tools/protoc_wrapper':
    Var('chromium_url') + '/chromium/src/tools/protoc_wrapper.git' + '@' + 'e9dbe1bf6a2a5d2d4973725874259eed587cf18d',
  'third_party/abseil-cpp': {
    'url': Var('chromium_url') + '/chromium/src/third_party/abseil-cpp.git' + '@' + '435e7d977fb36fb47854a4c552c0706dad0bd7cf',
    'condition': 'not build_with_chromium',
  },
  'third_party/fadec/src': {
    'url': Var('chromium_url') + '/external/github.com/aengelke/fadec.git' + '@' + 'c9f78f532b9004de278489019ab2c6c28ae9746e',
  },
  'third_party/disarm/src': {
    'url': Var('chromium_url') + '/external/github.com/aengelke/disarm.git' + '@' + '2d13d3f410a52daff1c5d8ef07d623332f372560',
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
                '-vpython-spec', '.vpython3',
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
               '--reproxy_cfg_template',
               'reproxy.cfg.template',
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
               '--reproxy_cfg_template',
               'reproxy.cfg.template',
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
