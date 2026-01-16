// Test that `register` should work with `require`
import '../common/index.mjs';
import { execPath } from 'node:process';
import fixtures from '../common/fixtures.js';
import { spawnSyncAndAssert } from '../common/child_process.js';

spawnSyncAndAssert(
  execPath,
  [
    '--no-warnings',
    '--require',
    fixtures.path('es-module-loaders/register-loader.cjs'),
    '--input-type=module',
    '--eval',
    'import "node:os";',
  ],
  {
    stdout: 'resolve passthru\nresolve passthru',
    stderr: '',
    trim: true,
  },
);
