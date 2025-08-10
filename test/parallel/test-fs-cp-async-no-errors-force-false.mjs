// This tests that it does not throw errors when directory is copied over and force is false.

import { mustCall, mustNotMutateObjectDeep } from '../common/index.mjs';
import assert from 'node:assert';
import { cp, cpSync, lstatSync, mkdirSync, writeFileSync } from 'node:fs';
import { join } from 'node:path';
import tmpdir from '../common/tmpdir.js';
import { assertDirEquivalent, nextdir } from '../common/fs.js';

tmpdir.refresh();

const src = nextdir();
mkdirSync(join(src, 'a', 'b'), mustNotMutateObjectDeep({ recursive: true }));
writeFileSync(join(src, 'README.md'), 'hello world', 'utf8');
const dest = nextdir();
cpSync(src, dest, mustNotMutateObjectDeep({ dereference: true, recursive: true }));
const initialStat = lstatSync(join(dest, 'README.md'));
cp(src, dest, {
  dereference: true,
  force: false,
  recursive: true,
}, mustCall((err) => {
  assert.strictEqual(err, null);
  assertDirEquivalent(src, dest);
  // File should not have been copied over, so access times will be identical:
  const finalStat = lstatSync(join(dest, 'README.md'));
  assert.strictEqual(finalStat.ctime.getTime(), initialStat.ctime.getTime());
}));
