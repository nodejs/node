
2.0.1 / 2015-05-28
==================

  * configure: try/catcht the semver range.test() call
  * README: update for visual studio 2013 (#510, @samccone)

2.0.0 / 2015-05-24
==================

  * configure: check for python2 executable by default, fallback to python
  * configure: don't clobber existing $PYTHONPATH
  * configure: use "path-array" for PYTHONPATH
  * gyp: fix for non-acsii userprofile name on Windows
  * gyp: always install into $PRODUCT_DIR
  * gyp: apply https://codereview.chromium.org/11361103/
  * gyp: don't use links at all, just copy the files instead
  * gyp: update gyp to e1c8fcf7
  * Updated README.md with updated Windows build info
  * Show URL when a download fails
  * package: add a "license" field
  * move HMODULE m declaration to top
  * Only add "-undefined dynamic_lookup" to loadable_module targets
  * win: optionally allow node.exe/iojs.exe to be renamed
  * Avoid downloading shasums if using tarPath
  * Add target name preprocessor define: `NODE_GYP_MODULE_NAME`
  * Show better error message in case of bad network settings
