'use strict';
// This tests that sync and async hooks can be mixed.

require('../common');
const { spawnSyncAndAssert } = require('../common/child_process');
const fixtures = require('../common/fixtures');

const app = fixtures.path('module-hooks', 'sync-and-async', 'app.js');

const testCases = [
  // When mixing sync and async hooks, the sync ones always run first.
  { preload: ['sync-customize', 'async-customize'], stdout: 'customized by sync hook' },
  { preload: ['async-customize', 'sync-customize'], stdout: 'customized by sync hook' },
  // It should still work when neither hook does any customization.
  { preload: ['sync-forward', 'async-forward'], stdout: 'Hello world' },
  { preload: ['async-forward', 'sync-forward'], stdout: 'Hello world' },
  // It should work when only one hook is customizing.
  { preload: ['sync-customize', 'async-forward'], stdout: 'customized by sync hook' },
  { preload: ['async-customize', 'sync-forward'], stdout: 'customized by async hook' },
];


for (const { preload, stdout } of testCases) {
  const importArgs = [];
  for (const p of preload) {
    importArgs.push('--import', fixtures.fileURL(`module-hooks/sync-and-async/${p}.js`));
  }
  spawnSyncAndAssert(process.execPath, [...importArgs, app], {
    stdout,
    trim: true,
  });
}
