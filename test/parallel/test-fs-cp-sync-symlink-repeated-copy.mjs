// This tests that repeatedly copying a directory containing a symlink
// to an unrelated directory succeeds.
// See https://github.com/nodejs/node/issues/65097.
import { mustNotMutateObjectDeep } from '../common/index.mjs';
import { nextdir } from '../common/fs.js';
import assert from 'node:assert';
import { cpSync, mkdirSync, realpathSync, symlinkSync } from 'node:fs';
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
cpSync(src, dest, mustNotMutateObjectDeep({ recursive: true }));
cpSync(src, dest, mustNotMutateObjectDeep({ recursive: true }));
assert.strictEqual(realpathSync(join(dest, 'link')), realpathSync(target));

// Also exercise the JavaScript (filter) path. The destination symlink
// already exists at this point, so this covers the repeated-copy case on
// the filtered code path as well.
cpSync(src, dest, mustNotMutateObjectDeep({ recursive: true, filter: () => true }));
cpSync(src, dest, mustNotMutateObjectDeep({ recursive: true, filter: () => true }));
assert.strictEqual(realpathSync(join(dest, 'link')), realpathSync(target));

// A symlink with a relative target pointing to an unrelated directory.
{
  const root = nextdir();
  const src = join(root, 'src');
  const dest = join(root, 'dest');
  const target = join(root, 'target');
  mkdirSync(src, mustNotMutateObjectDeep({ recursive: true }));
  mkdirSync(target);
  symlinkSync('../target', join(src, 'link'));
  cpSync(src, dest, mustNotMutateObjectDeep({ recursive: true }));
  cpSync(src, dest, mustNotMutateObjectDeep({ recursive: true }));
  assert.strictEqual(realpathSync(join(dest, 'link')), realpathSync(target));

  // Same as above, exercising the JavaScript (filter) path.
  cpSync(src, dest, mustNotMutateObjectDeep({ recursive: true, filter: () => true }));
  cpSync(src, dest, mustNotMutateObjectDeep({ recursive: true, filter: () => true }));
  assert.strictEqual(realpathSync(join(dest, 'link')), realpathSync(target));
}
