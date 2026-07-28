// Test that `register` should accept URL objects as `parentURL`
import '../common/index.mjs';
import assert from 'node:assert';
import { execPath } from 'node:process';
import fixtures from '../common/fixtures.js';
import { spawnSyncAndAssert } from '../common/child_process.js';

spawnSyncAndAssert(
  execPath,
  [
    '--no-warnings',
    '--import',
    fixtures.fileURL('es-module-loaders/register-hooks-with-current-cwd-parent-url.mjs'),
    fixtures.path('es-module-loaders/register-loader-with-url-parenturl.mjs'),
  ],
  {
    cwd: fixtures.path('es-module-loaders/'),
  },
  {
    stdout(output) {
      assert.deepStrictEqual(output.split('\n').sort(), ['hooks initialize 1', '{"default":"foo"}'].sort());
    },
    stderr: '',
    trim: true,
  },
);
