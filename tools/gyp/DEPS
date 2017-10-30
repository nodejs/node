# DEPS file for gclient use in buildbot execution of gyp tests.
#
# (You don't need to use gclient for normal GYP development work.)

vars = {
  "chromium_git": "https://chromium.googlesource.com/",
}

deps = {
}

deps_os = {
  "win": {
    "third_party/cygwin":
      Var("chromium_git") + "chromium/deps/cygwin@4fbd5b9",

    "third_party/python_26":
      Var("chromium_git") + "chromium/deps/python_26@5bb4080",

    "src/third_party/pefile":
       Var("chromium_git") + "external/pefile@72c6ae4",
  },
}
