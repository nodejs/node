'use strict';

// This test verifies that the `getAssetKeys()` function works correctly
// in a single executable application without any assets.

require('../common');

const {
  generateSEA,
  skipIfSingleExecutableIsNotSupported,
} = require('../common/sea');

skipIfSingleExecutableIsNotSupported();

const tmpdir = require('../common/tmpdir');

const {
  spawnSyncAndAssert,
} = require('../common/child_process');
const fixtures = require('../common/fixtures');

tmpdir.refresh();

const outputFile = generateSEA(fixtures.path('sea', 'asset-keys-empty'));

spawnSyncAndAssert(
  outputFile,
  {
    env: {
      ...process.env,
      NODE_DEBUG_NATIVE: 'SEA',
    },
  },
  {
    stdout: /Asset keys: \[\]/,
  },
);
