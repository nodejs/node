# Note: The buildbots evaluate this file with CWD set to the parent
# directory and assume that the root of the checkout is in ./v8/, so
# all paths in here must match this assumption.

vars = {
  "chromium_trunk": "https://src.chromium.org/svn/trunk",

  "buildtools_revision": "fb782d4369d5ae04f17a2fceef7de5a63e50f07b",
}

deps = {
  # Remember to keep the revision in sync with the Makefile.
  "v8/build/gyp":
    "http://gyp.googlecode.com/svn/trunk@1831",

  "v8/third_party/icu":
    Var("chromium_trunk") + "/deps/third_party/icu52@277999",

  "v8/buildtools":
    "https://chromium.googlesource.com/chromium/buildtools.git@" +
    Var("buildtools_revision"),

  "v8/testing/gtest":
    "http://googletest.googlecode.com/svn/trunk@692",

  "v8/testing/gmock":
    "http://googlemock.googlecode.com/svn/trunk@485",
}

deps_os = {
  "win": {
    "v8/third_party/cygwin":
      Var("chromium_trunk") + "/deps/third_party/cygwin@66844",

    "v8/third_party/python_26":
      Var("chromium_trunk") + "/tools/third_party/python_26@89111",
  }
}

include_rules = [
  # Everybody can use some things.
  "+include",
  "+unicode",
  "+third_party/fdlibm",
]

# checkdeps.py shouldn't check for includes in these directories:
skip_child_includes = [
  "build",
  "third_party",
]

hooks = [
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
    # A change to a .gyp, .gypi, or to GYP itself should run the generator.
    "pattern": ".",
    "action": ["python", "v8/build/gyp_v8"],
  },
]
