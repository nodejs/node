// Test that all WebAssembly global types are properly handled
import '../common/index.mjs';
import { spawnSyncAndAssert } from '../common/child_process.js';
import * as fixtures from '../common/fixtures.js';

spawnSyncAndAssert(
  process.execPath,
  ['--no-warnings', fixtures.path('es-modules/test-wasm-globals-all-types.mjs')],
  { stdout: '', stderr: '', trim: true }
);
