# Note: The buildbots evaluate this file with CWD set to the parent
# directory and assume that the root of the checkout is in ./v8/, so
# all paths in here must match this assumption.

vars = {
  "git_url": "https://chromium.googlesource.com",
}

deps = {
  "v8/build":
    Var("git_url") + "/chromium/src/build.git" + "@" + "59daf502c36f20b5c9292f4bd9af85791f8a5884",
  "v8/tools/gyp":
    Var("git_url") + "/external/gyp.git" + "@" + "702ac58e477214c635d9b541932e75a95d349352",
  "v8/third_party/icu":
    Var("git_url") + "/chromium/deps/icu.git" + "@" + "2341038bf72869a5683a893a2b319a48ffec7f62",
  "v8/third_party/instrumented_libraries":
    Var("git_url") + "/chromium/src/third_party/instrumented_libraries.git" + "@" + "f15768d7fdf68c0748d20738184120c8ab2e6db7",
  "v8/buildtools":
    Var("git_url") + "/chromium/buildtools.git" + "@" + "adb8bf4e8fc92aa1717bf151b862d58e6f27c4f2",
  "v8/base/trace_event/common":
    Var("git_url") + "/chromium/src/base/trace_event/common.git" + "@" + "315bf1e2d45be7d53346c31cfcc37424a32c30c8",
  "v8/third_party/WebKit/Source/platform/inspector_protocol":
    Var("git_url") + "/chromium/src/third_party/WebKit/Source/platform/inspector_protocol.git" + "@" + "547960151fb364dd9a382fa79ffc9abfb184e3d1",
  "v8/third_party/jinja2":
    Var("git_url") + "/chromium/src/third_party/jinja2.git" + "@" + "2222b31554f03e62600cd7e383376a7c187967a1",
  "v8/third_party/markupsafe":
    Var("git_url") + "/chromium/src/third_party/markupsafe.git" + "@" + "484a5661041cac13bfc688a26ec5434b05d18961",
  "v8/tools/mb":
    Var('git_url') + '/chromium/src/tools/mb.git' + '@' + "99788b8b516c44d7db25cfb68695bc234fdee5ed",
  "v8/tools/swarming_client":
    Var('git_url') + '/external/swarming.client.git' + '@' + "e4288c3040a32f2e7ad92f957668f2ee3d36e5a6",
  "v8/testing/gtest":
    Var("git_url") + "/external/github.com/google/googletest.git" + "@" + "6f8a66431cb592dad629028a50b3dd418a408c87",
  "v8/testing/gmock":
    Var("git_url") + "/external/googlemock.git" + "@" + "0421b6f358139f02e102c9c332ce19a33faf75be",
  "v8/test/benchmarks/data":
    Var("git_url") + "/v8/deps/third_party/benchmarks.git" + "@" + "05d7188267b4560491ff9155c5ee13e207ecd65f",
  "v8/test/mozilla/data":
    Var("git_url") + "/v8/deps/third_party/mozilla-tests.git" + "@" + "f6c578a10ea707b1a8ab0b88943fe5115ce2b9be",
  "v8/test/simdjs/data": Var("git_url") + "/external/github.com/tc39/ecmascript_simd.git" + "@" + "baf493985cb9ea7cdbd0d68704860a8156de9556",
  "v8/test/test262/data":
    Var("git_url") + "/external/github.com/tc39/test262.git" + "@" + "88bc7fe7586f161201c5f14f55c9c489f82b1b67",
  "v8/test/test262/harness":
    Var("git_url") + "/external/github.com/test262-utils/test262-harness-py.git" + "@" + "cbd968f54f7a95c6556d53ba852292a4c49d11d8",
  "v8/tools/clang":
    Var("git_url") + "/chromium/src/tools/clang.git" + "@" + "3afb04a8153e40ff00f9eaa14337851c3ab4a368",
}

deps_os = {
  "android": {
    "v8/third_party/android_tools":
      Var("git_url") + "/android_tools.git" + "@" + "af1c5a4cd6329ccdcf8c2bc93d9eea02f9d74869",
  },
  "win": {
    "v8/third_party/cygwin":
      Var("git_url") + "/chromium/deps/cygwin.git" + "@" + "c89e446b273697fadf3a10ff1007a97c0b7de6df",
  }
}

recursedeps = [ 'v8/third_party/android_tools' ]

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
    'action': ['python', 'v8/gypfiles/vs_toolchain.py', 'update'],
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
    "action": ["python", "v8/gypfiles/gyp_v8"],
  },
]
