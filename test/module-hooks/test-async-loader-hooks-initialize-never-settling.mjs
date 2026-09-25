// Test that `initialize` returning never-settling promise should be handled
import '../common/index.mjs';
import { execPath } from 'node:process';
import fixtures from '../common/fixtures.js';
import { spawnSyncAndAssert } from '../common/child_process.js';

spawnSyncAndAssert(
  execPath,
  [
    '--no-warnings',
    fixtures.path('module-hooks/register-loader-initialize-never-settling.mjs'),
  ],
  {
    stdout: 'caught ERR_ASYNC_LOADER_REQUEST_NEVER_SETTLED',
    stderr: '',
    trim: true,
  },
);
