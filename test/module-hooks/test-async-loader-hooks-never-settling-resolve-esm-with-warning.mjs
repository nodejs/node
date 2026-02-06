// Test top-level await of a never-settling resolve with warning in ESM files
import '../common/index.mjs';
import { execPath } from 'node:process';
import fixtures from '../common/fixtures.js';
import { spawnSyncAndExit } from '../common/child_process.js';

spawnSyncAndExit(
  execPath,
  [
    '--experimental-loader',
    fixtures.fileURL('es-module-loaders/never-settling-resolve-step/loader.mjs'),
    fixtures.path('es-module-loaders/never-settling-resolve-step/never-resolve.mjs'),
  ],
  {
    status: 13,
    signal: null,
    stdout: /^should be output$/,
    stderr: /Warning: Detected unsettled top-level await at.+never-resolve\.mjs:5/,
    trim: true,
  },
);
