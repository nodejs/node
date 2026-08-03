// This tests that cpSync throws error if attempt is made to copy to subdirectory of self.
import '../common/index.mjs';
import assert from 'node:assert';
import { cpSync } from 'node:fs';
import tmpdir from '../common/tmpdir.js';
import fixtures from '../common/fixtures.js';
tmpdir.refresh();

const src = fixtures.path('copy/kitchen-sink');
const dest = fixtures.path('copy/kitchen-sink/a');
assert.throws(
  () => cpSync(src, dest),
  { code: 'ERR_FS_CP_EINVAL' }
);
