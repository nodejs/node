// Test that loader hooks should handle undefined thrown from top-level
import '../common/index.mjs';
import { execPath } from 'node:process';
import fixtures from '../common/fixtures.js';
import { spawnSyncAndExit } from '../common/child_process.js';

spawnSyncAndExit(
  execPath,
  [
    '--no-warnings',
    '--experimental-loader',
    fixtures.fileURL('es-module-loaders/loader-throw-undefined.mjs'),
    fixtures.path('empty.js'),
  ],
  {
    status: 1,
    signal: null,
    stdout: '',
    stderr: /^undefined$/m,
    trim: true,
  },
);
