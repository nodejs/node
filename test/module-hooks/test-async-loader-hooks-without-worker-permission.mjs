// Test that loader hooks should not work without worker permission
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
    fixtures.fileURL('empty.js'),
    fixtures.path('es-modules/esm-top-level-await.mjs'),
  ],
  {
    status: 1,
    signal: null,
    stdout: '',
    stderr: /Error: Access to this API has been restricted/,
    trim: true,
  },
);
