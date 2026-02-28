// Test that loader hooks should not leak internals or expose import.meta.resolve
import '../common/index.mjs';
import { execPath } from 'node:process';
import fixtures from '../common/fixtures.js';
import { spawnSyncAndAssert } from '../common/child_process.js';

spawnSyncAndAssert(
  execPath,
  [
    '--no-warnings',
    '--experimental-loader',
    fixtures.fileURL('es-module-loaders/loader-edge-cases.mjs'),
    fixtures.path('empty.js'),
  ],
  {
    stdout: '',
    stderr: '',
    trim: true,
  },
);
