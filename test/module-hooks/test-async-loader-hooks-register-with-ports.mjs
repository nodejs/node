// Test that loader should allow communicating with loader via `register` ports
import '../common/index.mjs';
import { execPath } from 'node:process';
import fixtures from '../common/fixtures.js';
import { spawnSyncAndAssert } from '../common/child_process.js';

spawnSyncAndAssert(
  execPath,
  [
    '--no-warnings',
    fixtures.path('module-hooks/register-loader-with-ports.mjs'),
  ],
  {
    stdout: 'register undefined\nmessage initialize\nmessage resolve node:os',
    stderr: '',
    trim: true,
  },
);
