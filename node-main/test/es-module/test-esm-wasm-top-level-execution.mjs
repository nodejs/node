// Test that WASM modules support top-level execution
import '../common/index.mjs';
import { spawnSyncAndAssert } from '../common/child_process.js';
import * as fixtures from '../common/fixtures.js';

spawnSyncAndAssert(
  process.execPath,
  ['--no-warnings', fixtures.path('es-modules/top-level-wasm.wasm')],
  {
    stdout: '[Object: null prototype] { prop: \'hello world\' }',
    stderr: '',
    trim: true
  }
);
