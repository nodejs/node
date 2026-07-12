// This tests that cp() copies timestamps from src to dest if preserveTimestamps is true.

import { mustCall } from '../common/index.mjs';
import { nextdir, assertDirEquivalent } from '../common/fs.js';
import assert from 'node:assert';
import { cp, lstatSync } from 'node:fs';
import { join } from 'node:path';
import tmpdir from '../common/tmpdir.js';
import fixtures from '../common/fixtures.js';

tmpdir.refresh();

const src = fixtures.path('copy/kitchen-sink');
const dest = nextdir();
cp(src, dest, {
  preserveTimestamps: true,
  recursive: true
}, mustCall((err) => {
  assert.strictEqual(err, null);
  assertDirEquivalent(src, dest);
  const srcStat = lstatSync(join(src, 'index.js'));
  const destStat = lstatSync(join(dest, 'index.js'));
  assert.strictEqual(srcStat.mtime.getTime(), destStat.mtime.getTime());
}));
