// Test that the package map flag has no impact when not set and emits a warning when used.
import '../common/index.mjs';
import assert from 'node:assert';
import { spawnSyncAndAssert } from '../common/child_process.js';
import * as fixtures from '../common/fixtures.mjs';

const packageMapPath = fixtures.path('package-map/package-map.json');

// No impact when flag not set.
spawnSyncAndAssert(process.execPath, [
  '--no-warnings',
  '--input-type=module',
  '--eval', `import fs from 'node:fs'; console.log('ok');`,
], { stdout: /ok/, trim: true });

// Emits experimental warning on first use.
spawnSyncAndAssert(process.execPath, [
  '--experimental-package-map', packageMapPath,
  '--input-type=module',
  '--eval', `import dep from 'dep-a';`,
], { cwd: fixtures.path('package-map/root') }, {
  stderr(output) {
    assert.match(output, /ExperimentalWarning/);
    assert.match(output, /Package map/i);
  },
});
