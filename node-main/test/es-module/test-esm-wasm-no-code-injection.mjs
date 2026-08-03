// Test that code injection through export names is not allowed
import '../common/index.mjs';
import { spawnSyncAndAssert } from '../common/child_process.js';
import * as fixtures from '../common/fixtures.js';

spawnSyncAndAssert(
  process.execPath,
  ['--no-warnings', fixtures.path('es-modules/export-name-code-injection.wasm')],
  { stdout: '', stderr: '', trim: true }
);
