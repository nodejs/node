'use strict';
// Test that imported CJS uses the format and source from synchronous load hooks,
// with and without asynchronous hooks from module.register(), when the resolve
// hooks don't short-circuit, or the module is not on disk. When the load hooks
// don't call nextLoad(), the module also gets the full CommonJS require().
// Refs: https://github.com/nodejs/node/issues/63060
// Refs: https://github.com/nodejs/node/issues/66998

require('../common');
const { spawnSyncAndAssert } = require('../common/child_process');
const fixtures = require('../common/fixtures');

const main = fixtures.path('module-hooks', 'import-cjs-custom-format', 'main.js');
const asyncForward = fixtures.fileURL('module-hooks', 'sync-and-async', 'async-forward.js').href;

const stdout = [
  '{"name":"data.json","hasRequireCache":true}',
  '{"name":"module.mjs","hasRequireCache":true}',
  '{"name":"missing.cjs","hasRequireCache":true}',
  '{"name":"data URL","hasRequireCache":true}',
  '{"name":"mutated.mjs"}',
  '{"name":"format-only.mjs"}',
].join('\n');

for (const preload of [[], ['--import', asyncForward]]) {
  spawnSyncAndAssert(process.execPath, [...preload, main], {
    stdout,
    trim: true,
  });
}
