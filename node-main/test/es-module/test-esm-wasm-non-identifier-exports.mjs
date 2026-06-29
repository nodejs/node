// Test that WASM modules can have non-identifier export names
import '../common/index.mjs';
import { spawnSyncAndAssert } from '../common/child_process.js';
import * as fixtures from '../common/fixtures.js';

spawnSyncAndAssert(
  process.execPath,
  ['--no-warnings', fixtures.path('es-modules/test-wasm-non-identifier-exports.mjs')],
  { stdout: '', stderr: '', trim: true }
);
