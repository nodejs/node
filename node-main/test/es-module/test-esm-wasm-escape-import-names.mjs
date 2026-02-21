// Test that import names are properly escaped
import '../common/index.mjs';
import { spawnSyncAndAssert } from '../common/child_process.js';
import * as fixtures from '../common/fixtures.js';

spawnSyncAndAssert(
  process.execPath,
  ['--no-warnings', fixtures.path('es-modules/test-wasm-escape-import-names.mjs')],
  { stdout: '', stderr: '', trim: true }
);
