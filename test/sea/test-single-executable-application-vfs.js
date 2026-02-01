'use strict';

// This tests the SEA VFS integration - sea.getVfs() and sea.hasAssets()

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
  },
);
