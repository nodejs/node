// Test never-settling load in CJS files
import '../common/index.mjs';
import { execPath } from 'node:process';
import fixtures from '../common/fixtures.js';
import { spawnSyncAndAssert } from '../common/child_process.js';

spawnSyncAndAssert(
  execPath,
  [
    '--no-warnings',
    '--experimental-loader',
    fixtures.fileURL('es-module-loaders/never-settling-resolve-step/loader.mjs'),
    fixtures.path('es-module-loaders/never-settling-resolve-step/never-load.cjs'),
  ],
  {
    stdout: /^should be output$/,
    stderr: '',
    trim: true,
  },
);
