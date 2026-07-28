// Test that `register` should work with cjs
import '../common/index.mjs';
import assert from 'node:assert';
import { execPath } from 'node:process';
import fixtures from '../common/fixtures.js';
import { spawnSyncAndAssert } from '../common/child_process.js';

spawnSyncAndAssert(
  execPath,
  [
    '--no-warnings',
    fixtures.path('module-hooks/register-loader-with-cjs.cjs'),
  ],
  {
    stdout(output) {
      assert.deepStrictEqual(output.split('\n').sort(), ['hooks initialize 1', '{"default":"foo"}', ''].sort());
    },
    stderr: '',
  },
);
