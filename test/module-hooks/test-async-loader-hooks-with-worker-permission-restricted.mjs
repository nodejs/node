// Test that loader hooks should not allow spawning workers if restricted by CLI flags
import '../common/index.mjs';
import { execPath } from 'node:process';
import fixtures from '../common/fixtures.js';
import { spawnSyncAndExit } from '../common/child_process.js';

spawnSyncAndExit(
  execPath,
  [
    '--no-warnings',
    '--permission',
    '--allow-fs-read',
    '*',
    '--experimental-loader',
    fixtures.fileURL('es-module-loaders/loader-worker-spawn.mjs'),
    fixtures.path('es-modules/esm-top-level-await.mjs'),
  ],
  {
    status: 1,
    signal: null,
    stdout: '',
    stderr: /code: 'ERR_ACCESS_DENIED'/,
    trim: true,
  },
);
