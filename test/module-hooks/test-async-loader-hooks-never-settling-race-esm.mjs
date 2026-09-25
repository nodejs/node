// Test top-level await of a race of never-settling hooks in ESM files
import '../common/index.mjs';
import { execPath } from 'node:process';
import fixtures from '../common/fixtures.js';
import { spawnSyncAndAssert } from '../common/child_process.js';

spawnSyncAndAssert(
  execPath,
  [
    '--no-warnings',
    '--experimental-loader',
    fixtures.fileURL('es-module-loaders/never-settling-resolve-step/loader.mjs'),
    fixtures.path('es-module-loaders/never-settling-resolve-step/race.mjs'),
  ],
  {
    stdout: /^true$/,
    stderr: '',
    trim: true,
  },
);
