// This tests that cp() returns error if errorOnExist is true, force is false, and file or folder copied over.

import { mustCall, mustNotMutateObjectDeep } from '../common/index.mjs';
import { nextdir } from '../common/fs.js';
import assert from 'node:assert';
import { cp, cpSync } from 'node:fs';
import tmpdir from '../common/tmpdir.js';
import fixtures from '../common/fixtures.js';

tmpdir.refresh();

const src = fixtures.path('copy/kitchen-sink');
const dest = nextdir();
cpSync(src, dest, mustNotMutateObjectDeep({ recursive: true }));
cp(src, dest, {
  dereference: true,
  errorOnExist: true,
  force: false,
  recursive: true,
}, mustCall((err) => {
  assert.strictEqual(err.code, 'ERR_FS_CP_EEXIST');
}));
