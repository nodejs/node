// Test that error is thrown for dynamic source phase imports not defined
import '../common/index.mjs';
import { spawnSyncAndAssert } from '../common/child_process.js';
import * as fixtures from '../common/fixtures.js';

spawnSyncAndAssert(
  process.execPath,
  ['--no-warnings', fixtures.path('es-modules/test-wasm-source-phase-not-defined-dynamic.mjs')],
  { stdout: '', stderr: '', trim: true }
);
