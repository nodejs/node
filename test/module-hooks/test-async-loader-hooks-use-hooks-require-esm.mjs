// Test that loader should use hooks for require of ESM
import '../common/index.mjs';
import { execPath } from 'node:process';
import fixtures from '../common/fixtures.js';
import { spawnSyncAndAssert } from '../common/child_process.js';

spawnSyncAndAssert(
  execPath,
  [
    '--no-experimental-require-module',
    '--import',
    fixtures.fileURL('es-module-loaders/builtin-named-exports.mjs'),
    fixtures.path('es-modules/require-esm-throws-with-loaders.js'),
  ],
  {
    stdout: '',
    stderr: '',
    trim: true,
  },
);
