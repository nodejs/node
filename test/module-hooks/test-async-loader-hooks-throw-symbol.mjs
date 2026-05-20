// Test that loader hooks should handle symbol thrown from top-level
import '../common/index.mjs';
import { execPath } from 'node:process';
import fixtures from '../common/fixtures.js';
import { spawnSyncAndExit } from '../common/child_process.js';

spawnSyncAndExit(
  execPath,
  [
    '--experimental-loader',
    fixtures.fileURL('es-module-loaders/loader-throw-symbol.mjs'),
    fixtures.path('empty.js'),
  ],
  {
    status: 1,
    signal: null,
    stdout: '',  // Throwing a symbol doesn't produce any output
    trim: true,
  },
);
