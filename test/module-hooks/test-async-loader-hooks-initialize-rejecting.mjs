// Test that `initialize` returning rejecting promise should be handled
import '../common/index.mjs';
import { execPath } from 'node:process';
import fixtures from '../common/fixtures.js';
import { spawnSyncAndExit } from '../common/child_process.js';

spawnSyncAndExit(
  execPath,
  [
    '--no-warnings',
    '--experimental-loader',
    fixtures.fileURL('es-module-loaders/loader-initialize-rejecting.mjs'),
    '--input-type=module',
    '--eval',
    'import "node:os"',
  ],
  {
    status: 1,
    signal: null,
    stdout: '',
    stderr: /undefined$/m,
    trim: true,
  },
);
