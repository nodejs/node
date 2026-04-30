// Test top-level await of a never-settling load without warning in ESM files
import '../common/index.mjs';
import { execPath } from 'node:process';
import fixtures from '../common/fixtures.js';
import { spawnSyncAndExit } from '../common/child_process.js';

spawnSyncAndExit(
  execPath,
  [
    '--no-warnings',
    '--experimental-loader',
    fixtures.fileURL('es-module-loaders/never-settling-resolve-step/loader.mjs'),
    fixtures.path('es-module-loaders/never-settling-resolve-step/never-load.mjs'),
  ],
  {
    status: 13,
    signal: null,
    stdout: /^should be output$/,
    stderr: '',
    trim: true,
  },
);
