// Test that experimental warning is emitted for WASM module instances
import '../common/index.mjs';
import assert from 'node:assert';

import { spawnSyncAndAssert } from '../common/child_process.js';
import * as fixtures from '../common/fixtures.js';

spawnSyncAndAssert(
  process.execPath,
  [fixtures.path('es-modules/wasm-modules.mjs')],
  {
    stderr(output) {
      assert.match(output, /ExperimentalWarning/);
      assert.match(output, /Importing WebAssembly module instances/);
    }
  }
);
