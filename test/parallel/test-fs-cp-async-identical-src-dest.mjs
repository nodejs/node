// This tests that cp() returns error when src and dest are identical.

import { mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import { cp } from 'node:fs';
import tmpdir from '../common/tmpdir.js';
import fixtures from '../common/fixtures.js';

tmpdir.refresh();

const src = fixtures.path('copy/kitchen-sink');
cp(src, src, mustCall((err) => {
  assert.strictEqual(err.code, 'ERR_FS_CP_EINVAL');
}));
