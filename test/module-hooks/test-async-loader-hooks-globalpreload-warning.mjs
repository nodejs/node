// Test that `globalPreload` should emit warning
import '../common/index.mjs';
import assert from 'node:assert';
import { execPath } from 'node:process';
import fixtures from '../common/fixtures.js';
import { spawnSyncAndAssert } from '../common/child_process.js';

spawnSyncAndAssert(
  execPath,
  [
    '--experimental-loader',
    fixtures.fileURL('es-module-loaders/loader-globalpreload-only.mjs'),
    '--experimental-loader',
    fixtures.fileURL('es-module-loaders/loader-globalpreload-with-return.mjs'),
    fixtures.path('empty.js'),
  ],
  {
    stderr(output) {
      const matches = output.match(/`globalPreload` has been removed; use `initialize` instead/g);
      assert.strictEqual(matches.length, 1);
    },
  },
);
