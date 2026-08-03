// Test WASM module with invalid import name starting with 'wasm-js:'
import '../common/index.mjs';
import { spawnSyncAndAssert } from '../common/child_process.js';
import * as fixtures from '../common/fixtures.js';

spawnSyncAndAssert(
  process.execPath,
  ['--no-warnings', fixtures.path('es-modules/test-wasm-reject-wasm-js-import-names.mjs')],
  { status: 1, stderr: /Invalid Wasm import name/ }
);
