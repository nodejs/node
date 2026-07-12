// Test that it should be fine to call process.exit from the loader thread top-level
import '../common/index.mjs';
import { execPath } from 'node:process';
import fixtures from '../common/fixtures.js';
import { spawnSyncAndExit } from '../common/child_process.js';

spawnSyncAndExit(
  execPath,
  [
    '--no-warnings',
    '--experimental-loader',
    fixtures.fileURL('es-module-loaders/loader-exit-top-level.mjs'),
    fixtures.path('empty.js'),
  ],
  {
    status: 42,
    signal: null,
    stdout: '',
    stderr: '',
    trim: true,
  },
);
