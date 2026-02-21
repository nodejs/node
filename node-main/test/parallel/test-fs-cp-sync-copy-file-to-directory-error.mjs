// This tests that cpSync throws error if attempt is made to copy file to directory.
import { mustNotMutateObjectDeep, isInsideDirWithUnusualChars, skip } from '../common/index.mjs';
import { nextdir } from '../common/fs.js';
import assert from 'node:assert';
import { cpSync, mkdirSync } from 'node:fs';
import tmpdir from '../common/tmpdir.js';
import fixtures from '../common/fixtures.js';

tmpdir.refresh();

// See https://github.com/nodejs/node/pull/48409
if (isInsideDirWithUnusualChars) {
  skip('Test is borken in directories with unusual characters');
}

const src = fixtures.path('copy/kitchen-sink/README.md');
const dest = nextdir();
mkdirSync(dest, mustNotMutateObjectDeep({ recursive: true }));
assert.throws(
  () => cpSync(src, dest),
  { code: 'ERR_FS_CP_NON_DIR_TO_DIR' }
);
