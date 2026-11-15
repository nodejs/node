'use strict';

// This tests that source maps are correctly loaded from a Single Executable
// Application when "useVfs" is enabled. The `.map` file and the original
// sources are bundled as `"assets"` and served from inside the read-only
// virtual file system mounted by the SEA, so `findSourceMap()` and the
// source-mapped stack trace must resolve against the mount point instead of
// the real file system.
//
// The fixture (`test/fixtures/sea/vfs-sourcemap/`) enables source maps through
// `execArgv` (`--enable-source-maps`), verifies that `module.findSourceMap()`
// finds the bundled map, and then throws an error. This test asserts that:
//   1. the mapped stack trace for the throw frame references the ORIGINAL
//      source (`original.js`) rather than the generated `sea.js`, and
//   2. the original source line shown above the error is read back from the
//      VFS (it carries the original 4-space indentation, not the 2-space
//      indentation of the generated `sea.js`).
//
// The fixture's `sea.js.map` is a source-map-v3 identity mapping that is tied
// to the current content of `sea.js` and `original.js`: most lines are mapped
// one-to-one, and the `throw` line maps its column from 2 (the generated
// 2-space indentation) to 4 (the original 4-space indentation). If either
// fixture source is edited, regenerate the map so the columns stay in sync.

require('../common');

const {
  buildSEA,
  skipIfBuildSEAIsNotSupported,
} = require('../common/sea');

skipIfBuildSEAIsNotSupported();

const tmpdir = require('../common/tmpdir');
const { spawnSyncAndAssert } = require('../common/child_process');
const fixtures = require('../common/fixtures');

tmpdir.refresh();
const outputFile = buildSEA(fixtures.path('sea', 'vfs-sourcemap'));

spawnSyncAndAssert(
  outputFile,
  {
    env: {
      ...process.env,
      NODE_DEBUG_NATIVE: undefined,
    },
  },
  {
    // The SEA main script throws an uncaught error to produce the
    // source-mapped stack trace.
    status: 1,
    stdout: /source map loaded from the SEA VFS: .*\/original\.js/,
    stderr(stderr) {
      // The throw frame must be remapped to the original source bundled in
      // the VFS. Its column (5) matches the original 4-space indentation.
      if (!/at boom \(.*\/original\.js:\d+:\d+\)/.test(stderr)) {
        throw new Error(`the boom() frame was not source-mapped:\n${stderr}`);
      }
      // It must not still point at the generated sea.js.
      if (/at boom \(.*\/sea\.js:\d+:\d+\)/.test(stderr)) {
        throw new Error(`the boom() frame was not remapped to original.js:\n${stderr}`);
      }
      // The original source line is read from the VFS, so it shows the
      // original 4-space indentation (the generated sea.js uses 2 spaces).
      if (!/^ {4}throw new Error\('boom from the original source'\);$/m.test(stderr)) {
        throw new Error(`the original source line was not read from the VFS:\n${stderr}`);
      }
    },
  },
);
