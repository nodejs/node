// Test that WASM modules can load and export functions correctly
import '../common/index.mjs';
import { spawnSyncAndAssert } from '../common/child_process.js';
import * as fixtures from '../common/fixtures.js';

spawnSyncAndAssert(
  process.execPath,
  ['--no-warnings', fixtures.path('es-modules/test-wasm-load-exports.mjs')],
  { stdout: '', stderr: '', trim: true }
);
