// Test that `initialize` should execute in sequence
import '../common/index.mjs';
import { execPath } from 'node:process';
import fixtures from '../common/fixtures.js';
import { spawnSyncAndAssert } from '../common/child_process.js';

spawnSyncAndAssert(
  execPath,
  [
    '--no-warnings',
    fixtures.path('module-hooks/register-loader-in-sequence.mjs'),
  ],
  {
    stdout: 'hooks initialize 1\n' +
            'result 1 undefined\n' +
            'hooks initialize 2\n' +
            'result 2 undefined',
    trim: true,
    stderr: '',
  },
);
