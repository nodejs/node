// This tests that it does not fail if the same directory is copied to dest
// twice, when dereference is true, and force is false (fails silently).

import { mustCall, mustNotMutateObjectDeep } from '../common/index.mjs';
import assert from 'node:assert';
import { cp, cpSync, lstatSync } from 'node:fs';
import { join } from 'node:path';
import { nextdir } from '../common/fs.js';
import tmpdir from '../common/tmpdir.js';
import fixtures from '../common/fixtures.js';

tmpdir.refresh();

const src = fixtures.path('copy/kitchen-sink');
const dest = nextdir();
const destFile = join(dest, 'a/b/README2.md');
cpSync(src, dest, mustNotMutateObjectDeep({ dereference: true, recursive: true }));
cp(src, dest, {
  dereference: true,
  recursive: true
}, mustCall((err) => {
  assert.strictEqual(err, null);
  const stat = lstatSync(destFile);
  assert(stat.isFile());
}));
