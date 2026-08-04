// Regression test for source phase import identity with mixed eval/source
// phase imports of the same module in one parent.
import '../common/index.mjs';
import { spawnSyncAndAssert } from '../common/child_process.js';
import * as fixtures from '../common/fixtures.mjs';

spawnSyncAndAssert(
  process.execPath,
  ['--no-warnings', fixtures.path('es-modules/test-wasm-source-phase-identity.mjs')],
  { stdout: '', stderr: '', trim: true }
);
