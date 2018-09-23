
2.0.2 / 2015-07-14
==================

  * Use HTTPS for dist url (#656, @SonicHedgehog)
  * Merge pull request #648 from nevosegal/master
  * Merge pull request #650 from magic890/patch-1
  * Updated Installation section on README
  * Updated link to gyp user documentation
  * Fix download error message spelling (#643, @tomxtobin)
  * Merge pull request #637 from lygstate/master
  * Set NODE_GYP_DIR for addon.gypi to setting absolute path for
    src/win_delay_load_hook.c, and fixes of the long relative path issue on Win32.
    Fixes #636 (#637, @lygstate).

2.0.1 / 2015-05-28
==================

  * configure: try/catch the semver range.test() call
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
