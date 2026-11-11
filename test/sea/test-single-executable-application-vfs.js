'use strict';

// This tests the SEA VFS integration - the bundled assets are mounted as a
// virtual file system and the main script runs from inside the mount.

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
const outputFile = buildSEA(fixtures.path('sea', 'vfs'));

spawnSyncAndAssert(
  outputFile,
  {
    env: {
      ...process.env,
      NODE_DEBUG_NATIVE: undefined,
    },
  },
  {
    stdout: /All SEA VFS tests passed!/,
    stderr(stderr) {
      if (/ExperimentalWarning: VirtualFileSystem/.test(stderr)) {
        throw new Error('SEA VFS should not emit the public VirtualFileSystem warning');
      }
    },
  },
);

// A SEA rejects --vfs-load at startup, so it cannot take the mount point of
// the assets. --build-sea already refuses it in "execArgv", so pass it through
// --node-options instead.
const rejectedFile = buildSEA(fixtures.path('sea', 'vfs'), {
  configPath: 'sea-config-vfs-load-rejected.json',
});
spawnSyncAndAssert(
  rejectedFile,
  ['--node-options=--experimental-vfs --vfs-load=.'],
  {
    env: {
      ...process.env,
      NODE_DEBUG_NATIVE: undefined,
    },
  },
  {
    status: 9,
    stderr: /--vfs-load cannot be used in a single executable application/,
  },
);
