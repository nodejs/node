// This tests that repeatedly copying a directory containing a symlink
// to an unrelated directory succeeds.
// See https://github.com/nodejs/node/issues/65097.
import { mustCall, mustNotMutateObjectDeep } from '../common/index.mjs';
import { nextdir } from '../common/fs.js';
import assert from 'node:assert';
import { cp, mkdirSync, realpathSync, symlinkSync } from 'node:fs';
import { join } from 'node:path';

import tmpdir from '../common/tmpdir.js';
tmpdir.refresh();

const root = nextdir();
const src = join(root, 'src');
const dest = join(root, 'dest');
const target = join(root, 'target');
mkdirSync(src, mustNotMutateObjectDeep({ recursive: true }));
mkdirSync(target);
symlinkSync(target, join(src, 'link'));
cp(src, dest, mustNotMutateObjectDeep({ recursive: true }), mustCall((err) => {
  assert.ifError(err);
  cp(src, dest, mustNotMutateObjectDeep({ recursive: true }), mustCall((err) => {
    assert.ifError(err);
    assert.strictEqual(realpathSync(join(dest, 'link')), realpathSync(target));
  }));
}));

// A symlink with a relative target pointing to an unrelated directory.
{
  const root = nextdir();
  const src = join(root, 'src');
  const dest = join(root, 'dest');
  const target = join(root, 'target');
  mkdirSync(src, mustNotMutateObjectDeep({ recursive: true }));
  mkdirSync(target);
  symlinkSync('../target', join(src, 'link'));
  cp(src, dest, mustNotMutateObjectDeep({ recursive: true }), mustCall((err) => {
    assert.ifError(err);
    cp(src, dest, mustNotMutateObjectDeep({ recursive: true }), mustCall((err) => {
      assert.ifError(err);
      assert.strictEqual(realpathSync(join(dest, 'link')), realpathSync(target));
    }));
  }));
}
