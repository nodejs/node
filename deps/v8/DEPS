# Note: The buildbots evaluate this file with CWD set to the parent
# directory and assume that the root of the checkout is in ./v8/, so
# all paths in here must match this assumption.

deps = {
  # Remember to keep the revision in sync with the Makefile.
  "v8/build/gyp":
    "http://gyp.googlecode.com/svn/trunk@1831",

  "v8/third_party/icu":
    "https://src.chromium.org/chrome/trunk/deps/third_party/icu46@258359",
}

deps_os = {
  "win": {
    "v8/third_party/cygwin":
      "http://src.chromium.org/svn/trunk/deps/third_party/cygwin@66844",

    "v8/third_party/python_26":
      "http://src.chromium.org/svn/trunk/tools/third_party/python_26@89111",
  }
}

hooks = [
  {
    # A change to a .gyp, .gypi, or to GYP itself should run the generator.
    "pattern": ".",
    "action": ["python", "v8/build/gyp_v8"],
  },
]
