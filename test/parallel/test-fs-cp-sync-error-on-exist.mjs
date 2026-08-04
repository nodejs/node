// This tests that cpSync throws error if errorOnExist is true, force is false, and file or folder copied over.
import { mustNotMutateObjectDeep } from '../common/index.mjs';
import { nextdir } from '../common/fs.js';
import assert from 'node:assert';
import { cpSync } from 'node:fs';
import tmpdir from '../common/tmpdir.js';
import fixtures from '../common/fixtures.js';

tmpdir.refresh();

const src = fixtures.path('copy/kitchen-sink');
const dest = nextdir();
cpSync(src, dest, mustNotMutateObjectDeep({ recursive: true }));
assert.throws(
  () => cpSync(src, dest, {
    dereference: true,
    errorOnExist: true,
    force: false,
    recursive: true,
  }),
  { code: 'ERR_FS_CP_EEXIST' }
);
