// Test that error is thrown for static source phase imports not defined
import '../common/index.mjs';
import assert from 'node:assert';

import { spawnSyncAndAssert } from '../common/child_process.js';
import * as fixtures from '../common/fixtures.js';

const fileUrl = fixtures.fileURL('es-modules/wasm-source-phase.js').href;
spawnSyncAndAssert(
  process.execPath,
  ['--no-warnings', fixtures.path('es-modules/test-wasm-source-phase-not-defined-static.mjs')],
  {
    status: 1,
    stderr(output) {
      assert.match(output, /Source phase import object is not defined for module/);
      assert(output.includes(fileUrl));
    }
  }
);
