// This tests that cpSync throws error if attempt is made to copy src to dest when
// src is parent directory of the parent of dest.
import { mustNotMutateObjectDeep, isInsideDirWithUnusualChars, skip } from '../common/index.mjs';
import { nextdir } from '../common/fs.js';
import assert from 'node:assert';
import { cpSync, mkdirSync } from 'node:fs';

import tmpdir from '../common/tmpdir.js';
tmpdir.refresh();

// See https://github.com/nodejs/node/pull/48409
if (isInsideDirWithUnusualChars) {
  skip('Test is borken in directories with unusual characters');
}

const src = nextdir('a', tmpdir);
const destParent = nextdir('a/b', tmpdir);
const dest = nextdir('a/b/c', tmpdir);
mkdirSync(src);
mkdirSync(destParent);
mkdirSync(dest);
assert.throws(
  () => cpSync(src, dest, mustNotMutateObjectDeep({ recursive: true })),
  { code: 'ERR_FS_CP_EINVAL' },
);
