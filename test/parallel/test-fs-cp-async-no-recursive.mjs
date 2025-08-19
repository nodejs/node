// This tests that cp() returns error if directory copied without recursive flag.

import { mustCall } from '../common/index.mjs';
import { nextdir } from '../common/fs.js';
import assert from 'node:assert';
import { cp } from 'node:fs';
import tmpdir from '../common/tmpdir.js';
import fixtures from '../common/fixtures.js';

tmpdir.refresh();

const src = fixtures.path('copy/kitchen-sink');
const dest = nextdir();
cp(src, dest, mustCall((err) => {
  assert.strictEqual(err.code, 'ERR_FS_EISDIR');
}));
