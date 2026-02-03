// Test that `initialize` should be invoked correctly
import '../common/index.mjs';
import { execPath } from 'node:process';
import fixtures from '../common/fixtures.js';
import { spawnSyncAndAssert } from '../common/child_process.js';

spawnSyncAndAssert(
  execPath,
  [
    '--no-warnings',
    '--experimental-loader',
    fixtures.fileURL('es-module-loaders/hooks-initialize.mjs'),
    '--input-type=module',
    '--eval',
    'import os from "node:os";',
  ],
  {
    stdout: 'hooks initialize 1',
    stderr: '',
    trim: true,
  },
);
