'use strict';
// This tests that in the require() in imported CJS can retry loading an ESM with TLA
// twice and get the correct error both times.

require('../common');
const { spawnSyncAndAssert } = require('../common/child_process');
const fixtures = require('../common/fixtures');
const assert = require('assert');

spawnSyncAndAssert(
  process.execPath,
  ['--import', fixtures.fileURL('es-modules', 'import-require-tla-twice', 'hook.js'),
   fixtures.path('es-modules', 'import-require-tla-twice', 'require-tla.js'),
  ],
  {
    stdout(output) {
      const matches = output.matchAll(/e\.code === ERR_REQUIRE_ASYNC_MODULE true/g);
      assert.strictEqual([...matches].length, 2);
    }
  }
);
