// Test that it should be fine to call `process.removeAllListeners("beforeExit")`("beforeExit") from the main thread
import '../common/index.mjs';
import { execPath } from 'node:process';
import fixtures from '../common/fixtures.js';
import { spawnSyncAndAssert } from '../common/child_process.js';

spawnSyncAndAssert(
  execPath,
  [
    '--no-warnings',
    '--experimental-loader',
    fixtures.fileURL('es-module-loaders/loader-delayed-async-load.mjs'),
    '--input-type=module',
    '--eval',
    'setInterval(() => process.removeAllListeners("beforeExit"),1).unref();await import("data:text/javascript,")',
  ],
  {
    stdout: '',
    stderr: '',
    trim: true,
  },
);
