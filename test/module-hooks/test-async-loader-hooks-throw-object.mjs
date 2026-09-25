// Test that loader hooks should handle plain object thrown from top-level
import '../common/index.mjs';
import { execPath } from 'node:process';
import fixtures from '../common/fixtures.js';
import { spawnSyncAndExit } from '../common/child_process.js';

spawnSyncAndExit(
  execPath,
  [
    '--no-warnings',
    '--experimental-loader',
    fixtures.fileURL('es-module-loaders/loader-throw-object.mjs'),
    fixtures.path('empty.js'),
  ],
  {
    status: 1,
    signal: null,
    stdout: '',
    stderr: /^\{ fn: \[Function: fn\], symbol: Symbol\(symbol\), u: undefined \}$/m,
    trim: true,
  },
);
