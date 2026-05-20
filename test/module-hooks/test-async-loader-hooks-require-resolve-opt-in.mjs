// Test that loader should use ESM loader to respond to `require.resolve` calls when opting in
import '../common/index.mjs';
import { execPath } from 'node:process';
import fixtures from '../common/fixtures.js';
import { spawnSyncAndAssert } from '../common/child_process.js';

spawnSyncAndAssert(
  execPath,
  [
    '--no-warnings',
    '--experimental-loader',
    fixtures.fileURL('es-module-loaders/loader-load-commonjs-with-source.mjs'),
    '--experimental-loader',
    fixtures.fileURL('es-module-loaders/loader-resolve-passthru.mjs'),
    fixtures.path('require-resolve.js'),
  ],
  {
    stdout: 'resolve passthru\n'.repeat(10).trim(),
    stderr: '',
    trim: true,
  },
);
