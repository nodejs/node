// This tests that it makes file writeable when updating timestamp, if not writeable.

import { mustCall, mustNotMutateObjectDeep } from '../common/index.mjs';
import assert from 'node:assert';
import { cp, lstatSync, mkdirSync, writeFileSync } from 'node:fs';
import { join } from 'node:path';
import tmpdir from '../common/tmpdir.js';
import { assertDirEquivalent, nextdir } from '../common/fs.js';

tmpdir.refresh();

const src = nextdir();
mkdirSync(src, mustNotMutateObjectDeep({ recursive: true }));
const dest = nextdir();
mkdirSync(dest, mustNotMutateObjectDeep({ recursive: true }));
writeFileSync(join(src, 'foo.txt'), 'foo', mustNotMutateObjectDeep({ mode: 0o444 }));
cp(src, dest, {
  preserveTimestamps: true,
  recursive: true,
}, mustCall((err) => {
  assert.strictEqual(err, null);
  assertDirEquivalent(src, dest);
  const srcStat = lstatSync(join(src, 'foo.txt'));
  const destStat = lstatSync(join(dest, 'foo.txt'));
  assert.strictEqual(srcStat.mtime.getTime(), destStat.mtime.getTime());
}));
