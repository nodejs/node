// Test that loader hooks should allow spawning workers when allowed by CLI flags
import '../common/index.mjs';
import { execPath } from 'node:process';
import fixtures from '../common/fixtures.js';
import { spawnSyncAndAssert } from '../common/child_process.js';

spawnSyncAndAssert(
  execPath,
  [
    '--no-warnings',
    '--permission',
    '--allow-worker',
    '--allow-fs-read',
    '*',
    '--experimental-loader',
    fixtures.fileURL('es-module-loaders/loader-worker-spawn.mjs'),
    fixtures.path('es-modules/esm-top-level-await.mjs'),
  ],
  {
    stdout: /^1\n2$/,
    stderr: '',
    trim: true,
  },
);
