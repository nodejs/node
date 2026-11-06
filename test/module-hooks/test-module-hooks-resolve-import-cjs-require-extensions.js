'use strict';
// Test that when module.registerHooks() resolve hooks call nextResolve() and only
// override the format to 'commonjs', and the load hooks don't provide their own
// source, the imported CJS module is still loaded through require.extensions,
// with or without asynchronous hooks from module.register(). The asynchronous
// hooks run in the default step of the synchronous hooks, so passing through
// them should not count as a short-circuit.
// Refs: https://github.com/nodejs/node/issues/66998

require('../common');
const { spawnSyncAndAssert } = require('../common/child_process');
const fixtures = require('../common/fixtures');

const main = fixtures.path('module-hooks', 'import-cjs-require-extensions', 'main.js');
const asyncForward = fixtures.fileURL('module-hooks', 'sync-and-async', 'async-forward.js').href;

for (const preload of [[], ['--import', asyncForward]]) {
  for (const load of ['pass-through', 'spread', 'next-twice', 'nullish-source']) {
    spawnSyncAndAssert(process.execPath, [...preload, main, load], {
      stdout: 'transformed',
      trim: true,
    });
  }
}
