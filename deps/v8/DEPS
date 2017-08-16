# Note: The buildbots evaluate this file with CWD set to the parent
# directory and assume that the root of the checkout is in ./v8/, so
# all paths in here must match this assumption.

vars = {
  "chromium_url": "https://chromium.googlesource.com",
}

deps = {
  "v8/build":
    Var("chromium_url") + "/chromium/src/build.git" + "@" + "1caf3a69f3b0379c9fef2493aa1b3cda96e17d7b",
  "v8/tools/gyp":
    Var("chromium_url") + "/external/gyp.git" + "@" + "eb296f67da078ec01f5e3a9ea9cdc6d26d680161",
  "v8/third_party/icu":
    Var("chromium_url") + "/chromium/deps/icu.git" + "@" + "c844075aa0f1758d04f9192825f1b1e7e607992e",
  "v8/third_party/instrumented_libraries":
    Var("chromium_url") + "/chromium/src/third_party/instrumented_libraries.git" + "@" + "644afd349826cb68204226a16c38bde13abe9c3c",
  "v8/buildtools":
    Var("chromium_url") + "/chromium/buildtools.git" + "@" + "98f00fa10dbad2cdbb2e297a66c3d6d5bc3994f3",
  "v8/base/trace_event/common":
    Var("chromium_url") + "/chromium/src/base/trace_event/common.git" + "@" + "39a3450531fc73432e963db8668695d2e8f13053",
  "v8/third_party/jinja2":
    Var("chromium_url") + "/chromium/src/third_party/jinja2.git" + "@" + "d34383206fa42d52faa10bb9931d6d538f3a57e0",
  "v8/third_party/markupsafe":
    Var("chromium_url") + "/chromium/src/third_party/markupsafe.git" + "@" + "8f45f5cfa0009d2a70589bcda0349b8cb2b72783",
  "v8/tools/swarming_client":
    Var('chromium_url') + '/external/swarming.client.git' + '@' + "a941a089ff1000403078b74cb628eb430f07d271",
  "v8/testing/gtest":
    Var("chromium_url") + "/external/github.com/google/googletest.git" + "@" + "6f8a66431cb592dad629028a50b3dd418a408c87",
  "v8/testing/gmock":
    Var("chromium_url") + "/external/googlemock.git" + "@" + "0421b6f358139f02e102c9c332ce19a33faf75be",
  "v8/test/benchmarks/data":
    Var("chromium_url") + "/v8/deps/third_party/benchmarks.git" + "@" + "05d7188267b4560491ff9155c5ee13e207ecd65f",
  "v8/test/mozilla/data":
    Var("chromium_url") + "/v8/deps/third_party/mozilla-tests.git" + "@" + "f6c578a10ea707b1a8ab0b88943fe5115ce2b9be",
  "v8/test/test262/data":
    Var("chromium_url") + "/external/github.com/tc39/test262.git" + "@" + "230f9fc5688ce76bfaa99aba5f680a159eaac9e2",
  "v8/test/test262/harness":
    Var("chromium_url") + "/external/github.com/test262-utils/test262-harness-py.git" + "@" + "0f2acdd882c84cff43b9d60df7574a1901e2cdcd",
  "v8/tools/clang":
    Var("chromium_url") + "/chromium/src/tools/clang.git" + "@" + "05f306039aa5029fa88768690e5c512097419f9d",
  "v8/test/wasm-js":
    Var("chromium_url") + "/external/github.com/WebAssembly/spec.git" + "@" + "07fd6430f879d36928d179a62d9bdeed82286065",
}

deps_os = {
  "android": {
    "v8/third_party/android_tools":
      Var("chromium_url") + "/android_tools.git" + "@" + "cb6bc21107001e2f2eeee2707b482b2b755baf51",
    "v8/third_party/catapult":
      Var('chromium_url') + "/external/github.com/catapult-project/catapult.git" + "@" + "08a6e0ac161db7309d8f9cad0ccd38e0b1fd41e0",
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
    # Pull gold plugin if needed or requested via GYP_DEFINES.
    # Note: This must run before the clang update.
    'name': 'gold_plugin',
    'pattern': '.',
    'action': ['python', 'v8/gypfiles/download_gold_plugin.py'],
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
