// This tests that cpSync throws error if attempt is made to copy directory to file.
import { isInsideDirWithUnusualChars, mustNotMutateObjectDeep, skip } from '../common/index.mjs';
import { nextdir } from '../common/fs.js';
import assert from 'node:assert';
import { cpSync, mkdirSync } from 'node:fs';
import tmpdir from '../common/tmpdir.js';
import fixtures from '../common/fixtures.js';

// See https://github.com/nodejs/node/pull/48409
if (isInsideDirWithUnusualChars) {
  skip('Test is borken in directories with unusual characters');
}

tmpdir.refresh();

{
  const src = nextdir();
  mkdirSync(src, mustNotMutateObjectDeep({ recursive: true }));
  const dest = fixtures.path('copy/kitchen-sink/README.md');
  assert.throws(
    () => cpSync(src, dest),
    { code: 'ERR_FS_CP_DIR_TO_NON_DIR' }
  );
}
