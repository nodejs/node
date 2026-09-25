// Test that loader hooks should handle function thrown from top-level
import '../common/index.mjs';
import { execPath } from 'node:process';
import fixtures from '../common/fixtures.js';
import { spawnSyncAndExit } from '../common/child_process.js';

spawnSyncAndExit(
  execPath,
  [
    '--no-warnings',
    '--experimental-loader',
    fixtures.fileURL('es-module-loaders/loader-throw-function.mjs'),
    fixtures.path('empty.js'),
  ],
  {
    status: 1,
    signal: null,
    stdout: '',
    stderr: /^\[Function: fnName\]$/m,
    trim: true,
  },
);
