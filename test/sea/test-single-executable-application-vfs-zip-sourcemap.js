'use strict';

// This tests that source maps are correctly loaded from a Single Executable
// Application when the assets are served from a ZIP archive mounted through
// the VFS (`"vfsArchive"`). The `.map` file and the original sources live in
// the archive and are served from the mount point, so `findSourceMap()` and
// the source-mapped stack trace must resolve against the mount point instead
// of the real file system.
//
// The fixture (`test/fixtures/sea/vfs-zip-sourcemap/`) enables source maps
// through `execArgv` (`--enable-source-maps`), verifies that
// `module.findSourceMap()` finds the bundled map, and then throws an error.
// This test asserts that the throw frame is remapped to the ORIGINAL source
// (`original.js`) in the archive and that the original source line (with its
// original 4-space indentation) is read back from the VFS.
//
// The fixture's `sea.js.map` is a source-map-v3 identity mapping that is tied
// to the current content of `sea.js` and `original.js`: most lines are mapped
// one-to-one, and the `throw` line maps its column from 2 (the generated
// 2-space indentation) to 4 (the original 4-space indentation). If either
// fixture source is edited, regenerate the map so the columns stay in sync.

const common = require('../common');

const {
  buildSEA,
  skipIfBuildSEAIsNotSupported,
} = require('../common/sea');

skipIfBuildSEAIsNotSupported();

const tmpdir = require('../common/tmpdir');
const { spawnSyncAndAssert } = require('../common/child_process');
const fixtures = require('../common/fixtures');
const { createWriteStream } = require('fs');
const { pipeline } = require('stream/promises');
const { zipFiles } = require('zlib');

// Asserts that the SEA stderr contains a source-mapped stack trace for an
// error thrown in `boom()`.
function assertSourceMappedStderr(stderr) {
  // The throw frame must be remapped to the original source in the VFS.
  if (!/at boom \(.*\/original\.js:\d+:\d+\)/.test(stderr)) {
    throw new Error(`the boom() frame was not source-mapped:\n${stderr}`);
  }
  // It must not still point at the generated sea.js.
  if (/at boom \(.*\/sea\.js:\d+:\d+\)/.test(stderr)) {
    throw new Error(`the boom() frame was not remapped to original.js:\n${stderr}`);
  }
  // The original source line is read from the VFS, so it shows the original
  // 4-space indentation (the generated sea.js uses 2 spaces).
  if (!/^ {4}throw new Error\('boom from the original source'\);$/m.test(stderr)) {
    throw new Error(`the original source line was not read from the VFS:\n${stderr}`);
  }
}

async function main() {
  tmpdir.refresh();

  // Build the assets archive with the ZIP support of node:zlib. This is what
  // "vfsArchive" users do themselves before running --build-sea.
  const fixture = (...args) => fixtures.path('sea', 'vfs-zip-sourcemap', ...args);
  await pipeline(
    zipFiles([
      [fixture('sea.js.map'), 'sea.js.map'],
      [fixture('original.js'), 'original.js'],
    ]),
    createWriteStream(tmpdir.resolve('assets.zip')),
  );
}

main().then(common.mustCall(() => {
  const outputFile = buildSEA(fixtures.path('sea', 'vfs-zip-sourcemap'));

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
      stderr: assertSourceMappedStderr,
    },
  );
}));
