// Test that error is thrown for vm source phase static import
import '../common/index.mjs';
import { spawnSyncAndAssert } from '../common/child_process.js';
import * as fixtures from '../common/fixtures.js';

spawnSyncAndAssert(
  process.execPath,
  ['--no-warnings', '--experimental-vm-modules', fixtures.path('es-modules/test-wasm-vm-source-phase-static.mjs')],
  { status: 1, stderr: /Source phase import object is not defined for module/ }
);
