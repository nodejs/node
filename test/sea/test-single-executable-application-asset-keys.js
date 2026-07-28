'use strict';

// This test verifies that the `getAssetKeys()` function works correctly
// in a single executable application with assets.

require('../common');

const {
  buildSEA,
  skipIfBuildSEAIsNotSupported,
} = require('../common/sea');

skipIfBuildSEAIsNotSupported();

const tmpdir = require('../common/tmpdir');

const {
  spawnSyncAndAssert,
} = require('../common/child_process');
const fixtures = require('../common/fixtures');

tmpdir.refresh();

const outputFile = buildSEA(fixtures.path('sea', 'asset-keys'));

spawnSyncAndAssert(
  outputFile,
  {
    env: {
      ...process.env,
      NODE_DEBUG_NATIVE: 'SEA',
    },
  },
  {
    stdout: /Asset keys: \["asset-1\.txt","asset-2\.txt","asset-3\.txt"\]/,
  },
);
