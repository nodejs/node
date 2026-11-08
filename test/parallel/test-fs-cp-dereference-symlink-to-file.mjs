// This tests that cp() and cpSync() copy the target file of a symlink passed
// as src when dereference is true and recursive is not set.

import { mustCall, mustNotMutateObjectDeep } from '../common/index.mjs';
import { nextdir } from '../common/fs.js';
import assert from 'node:assert';
import {
  cp,
  cpSync,
  lstatSync,
  mkdirSync,
  readFileSync,
  symlinkSync,
  writeFileSync,
} from 'node:fs';
import { join } from 'node:path';
import tmpdir from '../common/tmpdir.js';

tmpdir.refresh();

const src = nextdir();
mkdirSync(src);
writeFileSync(join(src, 'foo.txt'), 'foo');
symlinkSync(join(src, 'foo.txt'), join(src, 'link.txt'));

const opts = mustNotMutateObjectDeep({ dereference: true });

{
  const dest = join(nextdir(), 'foo.txt');
  cpSync(join(src, 'link.txt'), dest, opts);
  assert(lstatSync(dest).isFile());
  assert.strictEqual(readFileSync(dest, 'utf8'), 'foo');
}

{
  const dest = join(nextdir(), 'foo.txt');
  cp(join(src, 'link.txt'), dest, opts, mustCall((err) => {
    assert.ifError(err);
    assert(lstatSync(dest).isFile());
    assert.strictEqual(readFileSync(dest, 'utf8'), 'foo');
  }));
}
