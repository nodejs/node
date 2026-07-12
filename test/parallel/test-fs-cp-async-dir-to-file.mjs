// This tests that cp() returns error if attempt is made to copy directory to file.

import { mustCall, mustNotMutateObjectDeep } from '../common/index.mjs';
import { nextdir } from '../common/fs.js';
import assert from 'node:assert';
import { cp, mkdirSync } from 'node:fs';
import tmpdir from '../common/tmpdir.js';
import fixtures from '../common/fixtures.js';

tmpdir.refresh();

const src = nextdir();
mkdirSync(src, mustNotMutateObjectDeep({ recursive: true }));
const dest = fixtures.path('copy/kitchen-sink/README.md');
cp(src, dest, mustCall((err) => {
  assert.strictEqual(err.code, 'ERR_FS_CP_DIR_TO_NON_DIR');
}));
