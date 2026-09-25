// This tests that cpSync with force: false and preserveTimestamps: true leaves
// the timestamps of the destination files it skips untouched.
import { mustNotMutateObjectDeep } from '../common/index.mjs';
import { nextdir } from '../common/fs.js';
import assert from 'node:assert';
import { cpSync, mkdirSync, readFileSync, statSync, utimesSync, writeFileSync } from 'node:fs';
import { join } from 'node:path';
import tmpdir from '../common/tmpdir.js';

tmpdir.refresh();

const src = nextdir();
mkdirSync(src, { recursive: true });
writeFileSync(join(src, 'file.txt'), 'src', 'utf8');
utimesSync(join(src, 'file.txt'), 1000, 1000);

// Without a filter the tree is copied in C++, with one it is walked in
// JavaScript.
for (const filter of [undefined, () => true]) {
  const dest = nextdir();
  mkdirSync(dest, { recursive: true });
  writeFileSync(join(dest, 'file.txt'), 'dest', 'utf8');
  utimesSync(join(dest, 'file.txt'), 2000, 2000);

  cpSync(src, dest, mustNotMutateObjectDeep({
    filter,
    force: false,
    preserveTimestamps: true,
    recursive: true,
  }));

  assert.strictEqual(readFileSync(join(dest, 'file.txt'), 'utf8'), 'dest');
  assert.strictEqual(statSync(join(dest, 'file.txt')).mtime.getTime(), 2000 * 1000);
}
