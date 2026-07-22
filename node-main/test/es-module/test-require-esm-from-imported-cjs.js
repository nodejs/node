'use strict';

// This tests that the require(esm) works from an imported CJS module
// when the require-d ESM is cached separately.

require('../common');
const { spawnSyncAndAssert } = require('../common/child_process');
const fixtures = require('../common/fixtures');

spawnSyncAndAssert(
  process.execPath,
  [
    '--experimental-require-module',
    '--import',
    fixtures.fileURL('es-modules', 'require-esm-in-cjs-cache', 'instrument-sync.js'),
    fixtures.path('es-modules', 'require-esm-in-cjs-cache', 'app.cjs'),
  ],
  {
    trim: true,
    stdout: / default: { hello: 'world' }/
  }
);

spawnSyncAndAssert(
  process.execPath,
  [
    '--experimental-require-module',
    '--import',
    fixtures.fileURL('es-modules', 'require-esm-in-cjs-cache', 'instrument.js'),
    fixtures.path('es-modules', 'require-esm-in-cjs-cache', 'app.cjs'),
  ],
  {
    trim: true,
    stdout: / default: { hello: 'world' }/
  }
);
