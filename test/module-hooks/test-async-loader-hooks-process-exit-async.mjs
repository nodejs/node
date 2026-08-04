// Test that it should be fine to call `process.exit` from a custom async hook
import '../common/index.mjs';
import { execPath } from 'node:process';
import fixtures from '../common/fixtures.js';
import { spawnSyncAndExit } from '../common/child_process.js';

spawnSyncAndExit(
  execPath,
  [
    '--no-warnings',
    '--experimental-loader',
    fixtures.fileURL('es-module-loaders/loader-exit-on-load.mjs'),
    '--input-type=module',
    '--eval',
    'import "data:exit"',
  ],
  {
    status: 42,
    signal: null,
    stdout: '',
    stderr: '',
    trim: true,
  },
);
