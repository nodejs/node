// Test that loader should handle mixed of opt-in modules and non-opt-in ones
import '../common/index.mjs';
import { execPath } from 'node:process';
import fixtures from '../common/fixtures.js';
import { spawnSyncAndAssert } from '../common/child_process.js';

spawnSyncAndAssert(
  execPath,
  [
    '--no-warnings',
    '--experimental-loader',
    fixtures.fileURL('es-module-loaders/loader-mixed-opt-in.mjs'),
    'entry-point',
  ],
  {
    stdout: 'Hello',
    stderr: '',
    trim: true,
  },
);
