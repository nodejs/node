'use strict';

require('../common');

const {
  generateSEA,
  skipIfSingleExecutableIsNotSupported,
} = require('../common/sea');

skipIfSingleExecutableIsNotSupported();

// This tests the snapshot support in single executable applications.

const tmpdir = require('../common/tmpdir');
const {
  spawnSyncAndAssert,
} = require('../common/child_process');
const fixtures = require('../common/fixtures');

{
  tmpdir.refresh();

  const outputFile = generateSEA(fixtures.path('sea', 'snapshot-worker'));

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
