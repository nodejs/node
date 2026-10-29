'use strict';

// This tests the SEA VFS integration with an ESM entry point - the bundled
// assets are mounted as a virtual file system and the ESM main script runs
// from inside the mount through the ESM loader.

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
const outputFile = buildSEA(fixtures.path('sea', 'vfs-esm'));

spawnSyncAndAssert(
  outputFile,
  {
    env: {
      ...process.env,
      NODE_DEBUG_NATIVE: undefined,
    },
  },
  {
    stdout: /All SEA VFS ESM tests passed!/,
    stderr(stderr) {
      if (/ExperimentalWarning: VirtualFileSystem/.test(stderr)) {
        throw new Error('SEA VFS should not emit the public VirtualFileSystem warning');
      }
    },
  },
);
