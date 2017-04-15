# Note: The buildbots evaluate this file with CWD set to the parent
# directory and assume that the root of the checkout is in ./v8/, so
# all paths in here must match this assumption.

vars = {
  "chromium_url": "https://chromium.googlesource.com",
}

deps = {
  "v8/build":
    Var("chromium_url") + "/chromium/src/build.git" + "@" + "f55127ddc3632dbd6fef285c71ab4cb103e08f0c",
  "v8/tools/gyp":
    Var("chromium_url") + "/external/gyp.git" + "@" + "e7079f0e0e14108ab0dba58728ff219637458563",
  "v8/third_party/icu":
    Var("chromium_url") + "/chromium/deps/icu.git" + "@" + "9cd2828740572ba6f694b9365236a8356fd06147",
  "v8/third_party/instrumented_libraries":
    Var("chromium_url") + "/chromium/src/third_party/instrumented_libraries.git" + "@" + "5b6f777da671be977f56f0e8fc3469a3ccbb4474",
  "v8/buildtools":
    Var("chromium_url") + "/chromium/buildtools.git" + "@" + "cb12d6e8641f0c9b0fbbfa4bf17c55c6c0d3c38f",
  "v8/base/trace_event/common":
    Var("chromium_url") + "/chromium/src/base/trace_event/common.git" + "@" + "06294c8a4a6f744ef284cd63cfe54dbf61eea290",
  "v8/third_party/jinja2":
    Var("chromium_url") + "/chromium/src/third_party/jinja2.git" + "@" + "b61a2c009a579593a259c1b300e0ad02bf48fd78",
  "v8/third_party/markupsafe":
    Var("chromium_url") + "/chromium/src/third_party/markupsafe.git" + "@" + "484a5661041cac13bfc688a26ec5434b05d18961",
  "v8/tools/swarming_client":
    Var('chromium_url') + '/external/swarming.client.git' + '@' + "ebc8dab6f8b8d79ec221c94de39a921145abd404",
  "v8/testing/gtest":
    Var("chromium_url") + "/external/github.com/google/googletest.git" + "@" + "6f8a66431cb592dad629028a50b3dd418a408c87",
  "v8/testing/gmock":
    Var("chromium_url") + "/external/googlemock.git" + "@" + "0421b6f358139f02e102c9c332ce19a33faf75be",
  "v8/test/benchmarks/data":
    Var("chromium_url") + "/v8/deps/third_party/benchmarks.git" + "@" + "05d7188267b4560491ff9155c5ee13e207ecd65f",
  "v8/test/mozilla/data":
    Var("chromium_url") + "/v8/deps/third_party/mozilla-tests.git" + "@" + "f6c578a10ea707b1a8ab0b88943fe5115ce2b9be",
  "v8/test/simdjs/data": Var("chromium_url") + "/external/github.com/tc39/ecmascript_simd.git" + "@" + "baf493985cb9ea7cdbd0d68704860a8156de9556",
  "v8/test/test262/data":
    Var("chromium_url") + "/external/github.com/tc39/test262.git" + "@" + "6a0f1189eb00d38ef9760cb65cbc41c066876cde",
  "v8/test/test262/harness":
    Var("chromium_url") + "/external/github.com/test262-utils/test262-harness-py.git" + "@" + "0f2acdd882c84cff43b9d60df7574a1901e2cdcd",
  "v8/tools/clang":
    Var("chromium_url") + "/chromium/src/tools/clang.git" + "@" + "f7ce1a5678e5addc015aed5f1e7734bbd2caac7c",
}

deps_os = {
  "android": {
    "v8/third_party/android_tools":
      Var("chromium_url") + "/android_tools.git" + "@" + "b43a6a289a7588b1769814f04dd6c7d7176974cc",
    "v8/third_party/catapult":
      Var('chromium_url') + "/external/github.com/catapult-project/catapult.git" + "@" + "143ba4ddeb05e6165fb8413c5f3f47d342922d24",
  },
  "win": {
    "v8/third_party/cygwin":
      Var("chromium_url") + "/chromium/deps/cygwin.git" + "@" + "c89e446b273697fadf3a10ff1007a97c0b7de6df",
  }
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
    "name": "wasm_fuzzer",
    "pattern": ".",
    "action": [ "download_from_google_storage",
                "--no_resume",
                "--no_auth",
                "-u",
                "--bucket", "v8-wasm-fuzzer",
                "-s", "v8/test/fuzzer/wasm.tar.gz.sha1",
    ],
  },
  {
    "name": "wasm_asmjs_fuzzer",
    "pattern": ".",
    "action": [ "download_from_google_storage",
                "--no_resume",
                "--no_auth",
                "-u",
                "--bucket", "v8-wasm-asmjs-fuzzer",
                "-s", "v8/test/fuzzer/wasm_asmjs.tar.gz.sha1",
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
