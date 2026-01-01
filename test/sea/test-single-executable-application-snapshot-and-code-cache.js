'use strict';

require('../common');

const {
  generateSEA,
  skipIfSingleExecutableIsNotSupported,
} = require('../common/sea');

skipIfSingleExecutableIsNotSupported();

// This tests "useCodeCache" is ignored when "useSnapshot" is true.

const tmpdir = require('../common/tmpdir');
const {
  spawnSyncAndAssert,
} = require('../common/child_process');
const fixtures = require('../common/fixtures');

{
  tmpdir.refresh();

  const outputFile = generateSEA(fixtures.path('sea', 'snapshot-and-code-cache'));

  spawnSyncAndAssert(
    outputFile,
    {
      env: {
        NODE_DEBUG_NATIVE: 'SEA,MKSNAPSHOT',
        ...process.env,
      },
    }, {
      stdout: 'Hello from snapshot',
      trim: true,
    });
}
