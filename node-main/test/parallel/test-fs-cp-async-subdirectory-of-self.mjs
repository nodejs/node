// This tests that cp() returns error if attempt is made to copy to subdirectory of self.

import { mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import { cp } from 'node:fs';
import fixtures from '../common/fixtures.js';

const src = fixtures.path('copy/kitchen-sink');
const dest = fixtures.path('copy/kitchen-sink/a');
cp(src, dest, mustCall((err) => {
  assert.strictEqual(err.code, 'ERR_FS_CP_EINVAL');
}));
