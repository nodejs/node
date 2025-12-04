// Test that source phase imports don't execute
import '../common/index.mjs';
import { spawnSyncAndAssert } from '../common/child_process.js';
import * as fixtures from '../common/fixtures.js';

spawnSyncAndAssert(
  process.execPath,
  ['--no-warnings', fixtures.path('es-modules/test-wasm-source-phase-no-execute.mjs')],
  { stdout: '', stderr: '', trim: true }
);
