// Test that loader should use CJS loader to respond to `require.resolve` calls by default
import '../common/index.mjs';
import { execPath } from 'node:process';
import fixtures from '../common/fixtures.js';
import { spawnSyncAndAssert } from '../common/child_process.js';

spawnSyncAndAssert(
  execPath,
  [
    '--no-warnings',
    '--experimental-loader',
    fixtures.fileURL('es-module-loaders/loader-resolve-passthru.mjs'),
    fixtures.path('require-resolve.js'),
  ],
  {
    stdout: 'resolve passthru',
    stderr: '',
    trim: true,
  },
);
