import '../common/index.mjs';
import { spawnSyncAndAssert } from '../common/child_process.js';
import * as fixtures from '../common/fixtures.mjs';

// Regression test for https://github.com/nodejs/node/issues/66806
spawnSyncAndAssert(process.execPath, [
  fixtures.path('es-modules', 'tla', 'dynamic-import-evaluating-async', 'main.mjs'),
], {
  stdout: /^ok 2 2\n$/,
  stderr: '',
});
