// This tests that cpSync throws error when src and dest are identical.
import '../common/index.mjs';
import assert from 'node:assert';
import { cpSync } from 'node:fs';
import tmpdir from '../common/tmpdir.js';
import fixtures from '../common/fixtures.js';

tmpdir.refresh();

const src = fixtures.path('copy/kitchen-sink');
assert.throws(
  () => cpSync(src, src),
  { code: 'ERR_FS_CP_EINVAL' }
);
