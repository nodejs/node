// Test that `register` should work with `import`
import '../common/index.mjs';
import { execPath } from 'node:process';
import fixtures from '../common/fixtures.js';
import { spawnSyncAndAssert } from '../common/child_process.js';

spawnSyncAndAssert(
  execPath,
  [
    '--no-warnings',
    '--import',
    fixtures.fileURL('es-module-loaders/register-loader.mjs'),
    '--input-type=module',
    '--eval',
    'import "node:os"',
  ],
  {
    stdout: 'resolve passthru',
    stderr: '',
    trim: true,
  },
);
