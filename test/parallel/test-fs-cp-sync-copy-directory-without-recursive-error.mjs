// This tests that cpSync throws error if directory copied without recursive flag.
import '../common/index.mjs';
import { nextdir } from '../common/fs.js';
import assert from 'node:assert';
import { cpSync } from 'node:fs';
import tmpdir from '../common/tmpdir.js';
import fixtures from '../common/fixtures.js';

tmpdir.refresh();

const src = fixtures.path('copy/kitchen-sink');
const dest = nextdir();
assert.throws(
  () => cpSync(src, dest),
  { code: 'ERR_FS_EISDIR' }
);
