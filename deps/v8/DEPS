# Note: The buildbots evaluate this file with CWD set to the parent
# directory and assume that the root of the checkout is in ./v8/, so
# all paths in here must match this assumption.

vars = {
  "chromium_url": "https://chromium.googlesource.com",
}

deps = {
  "v8/build":
    Var("chromium_url") + "/chromium/src/build.git" + "@" + "1808a907ce42f13b224c263e9843d718fc6d9c39",
  "v8/tools/gyp":
    Var("chromium_url") + "/external/gyp.git" + "@" + "eb296f67da078ec01f5e3a9ea9cdc6d26d680161",
  "v8/third_party/icu":
    Var("chromium_url") + "/chromium/deps/icu.git" + "@" + "dfa798fe694702b43a3debc3290761f22b1acaf8",
  "v8/third_party/instrumented_libraries":
    Var("chromium_url") + "/chromium/src/third_party/instrumented_libraries.git" + "@" + "644afd349826cb68204226a16c38bde13abe9c3c",
  "v8/buildtools":
    Var("chromium_url") + "/chromium/buildtools.git" + "@" + "5ad14542a6a74dd914f067b948c5d3e8d170396b",
  "v8/base/trace_event/common":
    Var("chromium_url") + "/chromium/src/base/trace_event/common.git" + "@" + "65d1d42a5df6c0a563a6fdfa58a135679185e5d9",
  "v8/third_party/jinja2":
    Var("chromium_url") + "/chromium/src/third_party/jinja2.git" + "@" + "d34383206fa42d52faa10bb9931d6d538f3a57e0",
  "v8/third_party/markupsafe":
    Var("chromium_url") + "/chromium/src/third_party/markupsafe.git" + "@" + "8f45f5cfa0009d2a70589bcda0349b8cb2b72783",
  "v8/tools/swarming_client":
    Var('chromium_url') + '/external/swarming.client.git' + '@' + "a56c2b39ca23bdf41458421a7f825ddbf3f43f28",
  "v8/testing/gtest":
    Var("chromium_url") + "/external/github.com/google/googletest.git" + "@" + "6f8a66431cb592dad629028a50b3dd418a408c87",
  "v8/testing/gmock":
    Var("chromium_url") + "/external/googlemock.git" + "@" + "0421b6f358139f02e102c9c332ce19a33faf75be",
  "v8/test/benchmarks/data":
    Var("chromium_url") + "/v8/deps/third_party/benchmarks.git" + "@" + "05d7188267b4560491ff9155c5ee13e207ecd65f",
  "v8/test/mozilla/data":
    Var("chromium_url") + "/v8/deps/third_party/mozilla-tests.git" + "@" + "f6c578a10ea707b1a8ab0b88943fe5115ce2b9be",
  "v8/test/test262/data":
    Var("chromium_url") + "/external/github.com/tc39/test262.git" + "@" + "1b911a8f8abf4cb63882cfbe72dcd4c82bb8ad91",
  "v8/test/test262/harness":
    Var("chromium_url") + "/external/github.com/test262-utils/test262-harness-py.git" + "@" + "0f2acdd882c84cff43b9d60df7574a1901e2cdcd",
  "v8/tools/clang":
    Var("chromium_url") + "/chromium/src/tools/clang.git" + "@" + "844603c1fcd47f578931b3ccd583e19f816a3842",
  "v8/test/wasm-js":
    Var("chromium_url") + "/external/github.com/WebAssembly/spec.git" + "@" + "aadd3a340c78e53078a7bb6c17cc30f105c2960c",
}

deps_os = {
  "android": {
    "v8/third_party/android_tools":
      Var("chromium_url") + "/android_tools.git" + "@" + "e9d4018e149d50172ed462a7c21137aa915940ec",
    "v8/third_party/catapult":
      Var('chromium_url') + "/external/github.com/catapult-project/catapult.git" + "@" + "44b022b2a09508ec025ae76a26308e89deb2cf69",
  },
}

recursedeps = [
  "v8/buildtools",
  "v8/third_party/android_tools",
]

include_rules = [
  # Everybody can use some things.
  "+include",
  "+unicode",
  "+third_party/fdlibm",
]

# checkdeps.py shouldn't check for includes in these directories:
skip_child_includes = [
  "build",
  "gypfiles",
  "third_party",
]

hooks = [
  {
    # This clobbers when necessary (based on get_landmines.py). It must be the
    # first hook so that other things that get/generate into the output
    # directory will not subsequently be clobbered.
    'name': 'landmines',
    'pattern': '.',
    'action': [
        'python',
        'v8/gypfiles/landmines.py',
    ],
  },
  # Pull clang-format binaries using checked-in hashes.
  {
    "name": "clang_format_win",
    "pattern": ".",
    "action": [ "download_from_google_storage",
                "--no_resume",
                "--platform=win32",
                "--no_auth",
                "--bucket", "chromium-clang-format",
                "-s", "v8/buildtools/win/clang-format.exe.sha1",
    ],
  },
  {
    "name": "clang_format_mac",
    "pattern": ".",
    "action": [ "download_from_google_storage",
                "--no_resume",
                "--platform=darwin",
                "--no_auth",
                "--bucket", "chromium-clang-format",
                "-s", "v8/buildtools/mac/clang-format.sha1",
    ],
  },
  {
    "name": "clang_format_linux",
    "pattern": ".",
    "action": [ "download_from_google_storage",
                "--no_resume",
                "--platform=linux*",
                "--no_auth",
                "--bucket", "chromium-clang-format",
                "-s", "v8/buildtools/linux64/clang-format.sha1",
    ],
  },
  {
    'name': 'gcmole',
    'pattern': '.',
    'action': [
        'python',
        'v8/tools/gcmole/download_gcmole_tools.py',
    ],
  },
  {
    'name': 'jsfunfuzz',
    'pattern': '.',
    'action': [
        'python',
        'v8/tools/jsfunfuzz/download_jsfunfuzz.py',
    ],
  },
  # Pull luci-go binaries (isolate, swarming) using checked-in hashes.
  {
    'name': 'luci-go_win',
    'pattern': '.',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--platform=win32',
                '--no_auth',
                '--bucket', 'chromium-luci',
                '-d', 'v8/tools/luci-go/win64',
    ],
  },
  {
    'name': 'luci-go_mac',
    'pattern': '.',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--platform=darwin',
                '--no_auth',
                '--bucket', 'chromium-luci',
                '-d', 'v8/tools/luci-go/mac64',
    ],
  },
  {
    'name': 'luci-go_linux',
    'pattern': '.',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--platform=linux*',
                '--no_auth',
                '--bucket', 'chromium-luci',
                '-d', 'v8/tools/luci-go/linux64',
    ],
  },
  # Pull GN using checked-in hashes.
  {
    "name": "gn_win",
    "pattern": ".",
    "action": [ "download_from_google_storage",
                "--no_resume",
                "--platform=win32",
                "--no_auth",
                "--bucket", "chromium-gn",
                "-s", "v8/buildtools/win/gn.exe.sha1",
    ],
  },
  {
    "name": "gn_mac",
    "pattern": ".",
    "action": [ "download_from_google_storage",
                "--no_resume",
                "--platform=darwin",
                "--no_auth",
                "--bucket", "chromium-gn",
                "-s", "v8/buildtools/mac/gn.sha1",
    ],
  },
  {
    "name": "gn_linux",
    "pattern": ".",
    "action": [ "download_from_google_storage",
                "--no_resume",
                "--platform=linux*",
                "--no_auth",
                "--bucket", "chromium-gn",
                "-s", "v8/buildtools/linux64/gn.sha1",
    ],
  },
  {
    "name": "wasm_spec_tests",
    "pattern": ".",
    "action": [ "download_from_google_storage",
                "--no_resume",
                "--no_auth",
                "-u",
                "--bucket", "v8-wasm-spec-tests",
                "-s", "v8/test/wasm-spec-tests/tests.tar.gz.sha1",
    ],
  },
  {
    "name": "closure_compiler",
    "pattern": ".",
    "action": [ "download_from_google_storage",
                "--no_resume",
                "--no_auth",
                "-u",
                "--bucket", "chromium-v8-closure-compiler",
                "-s", "v8/src/inspector/build/closure-compiler.tar.gz.sha1",
    ],
  },
  {
    # Downloads the current stable linux sysroot to build/linux/ if needed.
    # This sysroot updates at about the same rate that the chrome build deps
    # change.
    'name': 'sysroot',
    'pattern': '.',
    'action': [
        'python',
        'v8/build/linux/sysroot_scripts/install-sysroot.py',
        '--running-as-hook',
    ],
  },
  {
    # Pull sanitizer-instrumented third-party libraries if requested via
    # GYP_DEFINES.
    'name': 'instrumented_libraries',
    'pattern': '\\.sha1',
    'action': [
        'python',
        'v8/third_party/instrumented_libraries/scripts/download_binaries.py',
    ],
  },
  {
    # Update the Windows toolchain if necessary.
    'name': 'win_toolchain',
    'pattern': '.',
    'action': ['python', 'v8/build/vs_toolchain.py', 'update'],
  },
  # Pull binutils for linux, enabled debug fission for faster linking /
  # debugging when used with clang on Ubuntu Precise.
  # https://code.google.com/p/chromium/issues/detail?id=352046
  {
    'name': 'binutils',
    'pattern': 'v8/third_party/binutils',
    'action': [
        'python',
        'v8/third_party/binutils/download.py',
    ],
  },
  {
    # Pull clang if needed or requested via GYP_DEFINES.
    # Note: On Win, this should run after win_toolchain, as it may use it.
    'name': 'clang',
    'pattern': '.',
    'action': ['python', 'v8/tools/clang/scripts/update.py', '--if-needed'],
  },
  {
    # A change to a .gyp, .gypi, or to GYP itself should run the generator.
    "pattern": ".",
    "action": ["python", "v8/gypfiles/gyp_v8", "--running-as-hook"],
  },
]
