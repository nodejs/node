// This tests that cpSync throws error if parent directory of symlink in dest points to src.
import { mustNotMutateObjectDeep, isInsideDirWithUnusualChars, skip } from '../common/index.mjs';
import { nextdir } from '../common/fs.js';
import assert from 'node:assert';
import { cpSync, mkdirSync, symlinkSync } from 'node:fs';
import { join } from 'node:path';

import tmpdir from '../common/tmpdir.js';
tmpdir.refresh();

// See https://github.com/nodejs/node/pull/48409
if (isInsideDirWithUnusualChars) {
  skip('Test is borken in directories with unusual characters');
}

const src = nextdir();
mkdirSync(join(src, 'a'), mustNotMutateObjectDeep({ recursive: true }));
const dest = nextdir();
// Create symlink in dest pointing to src.
const destLink = join(dest, 'b');
mkdirSync(dest, mustNotMutateObjectDeep({ recursive: true }));
symlinkSync(src, destLink);
assert.throws(
  () => cpSync(src, join(dest, 'b', 'c')),
  { code: 'ERR_FS_CP_EINVAL' }
);
