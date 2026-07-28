'use strict';

// This tests that --import preload does not break CJS entry points that contains
// require cycles.

require('../common');
const fixtures = require('../common/fixtures');
const { spawnSyncAndAssert } = require('../common/child_process');

spawnSyncAndAssert(
  process.execPath,
  [
    '--import',
    fixtures.fileURL('import-require-cycle/preload.mjs'),
    fixtures.path('import-require-cycle/c.js'),
  ],
  {
    stdout: /cycle equality true/,
  }
);
