'use strict';

require('../common');

const {
  buildSEA,
  skipIfBuildSEAIsNotSupported,
} = require('../common/sea');

skipIfBuildSEAIsNotSupported();

// This tests "useCodeCache" is ignored when "useSnapshot" is true.

const tmpdir = require('../common/tmpdir');
const {
  spawnSyncAndAssert,
} = require('../common/child_process');
const fixtures = require('../common/fixtures');

{
  tmpdir.refresh();

  const outputFile = buildSEA(fixtures.path('sea', 'snapshot-and-code-cache'));

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
