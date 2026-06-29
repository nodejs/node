// Test that js-string builtins are supported
import '../common/index.mjs';
import { spawnSyncAndAssert } from '../common/child_process.js';
import * as fixtures from '../common/fixtures.js';

spawnSyncAndAssert(
  process.execPath,
  ['--no-warnings', fixtures.path('es-modules/test-wasm-js-string-builtins.mjs')],
  { stdout: '', stderr: '', trim: true }
);
