// Test that loader should support source maps in commonjs translator
import '../common/index.mjs';
import { execPath } from 'node:process';
import fixtures from '../common/fixtures.js';
import { spawnSyncAndExit } from '../common/child_process.js';

spawnSyncAndExit(
  execPath,
  [
    '--no-warnings',
    '--enable-source-maps',
    '--import',
    fixtures.fileURL('es-module-loaders/loader-load-source-maps.mjs'),
    fixtures.path('source-map/throw-on-require.js'),
  ],
  {
    status: 1,
    signal: null,
    stdout: '',
    stderr: /throw-on-require\.ts:9:9/,
    trim: true,
  },
);
