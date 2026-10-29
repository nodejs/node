// Test that `globalPreload` should not emit warning when `initialize` is supplied

import '../common/index.mjs';
import assert from 'node:assert';
import { execPath } from 'node:process';
import fixtures from '../common/fixtures.js';
import { spawnSyncAndAssert } from '../common/child_process.js';

spawnSyncAndAssert(
  execPath,
  [
    '--experimental-loader',
    fixtures.fileURL('es-module-loaders/loader-globalpreload-and-initialize.mjs'),
    fixtures.path('empty.js'),
  ],
  {
    stderr(output) {
      assert.doesNotMatch(output, /`globalPreload` has been removed; use `initialize` instead/);
    },
  },
);
