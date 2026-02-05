'use strict';

require('../common');

const {
  buildSEA,
  skipIfBuildSEAIsNotSupported,
} = require('../common/sea');

skipIfBuildSEAIsNotSupported();

// This tests the snapshot support in single executable applications.

const tmpdir = require('../common/tmpdir');
const {
  spawnSyncAndAssert,
} = require('../common/child_process');
const fixtures = require('../common/fixtures');

{
  tmpdir.refresh();

  const outputFile = buildSEA(fixtures.path('sea', 'snapshot-worker'));

  spawnSyncAndAssert(
    outputFile,
    {
      env: {
        NODE_DEBUG_NATIVE: 'SEA',
        ...process.env,
      },
    },
    {
      trim: true,
      stdout: 'Hello from Worker',
    },
  );
}
