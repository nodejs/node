// This tests that cpSync throws error if filter function is asynchronous.
import '../common/index.mjs';
import { nextdir } from '../common/fs.js';
import assert from 'node:assert';
import { cpSync, statSync } from 'node:fs';
import { setTimeout } from 'node:timers/promises';
import tmpdir from '../common/tmpdir.js';
import fixtures from '../common/fixtures.js';

tmpdir.refresh();

const src = fixtures.path('copy/kitchen-sink');
const dest = nextdir();
assert.throws(() => {
  cpSync(src, dest, {
    filter: async (path) => {
      await setTimeout(5, 'done');
      const pathStat = statSync(path);
      return pathStat.isDirectory() || path.endsWith('.js');
    },
    dereference: true,
    recursive: true,
  });
}, { code: 'ERR_INVALID_RETURN_VALUE' });
