// This tests that fs.promises.cp() allows async error to be caught.

import { mustNotMutateObjectDeep } from '../common/index.mjs';
import { nextdir } from '../common/fs.js';
import assert from 'node:assert';
import fs from 'node:fs';
import tmpdir from '../common/tmpdir.js';
import fixtures from '../common/fixtures.js';

tmpdir.refresh();

const src = fixtures.path('copy/kitchen-sink');
const dest = nextdir();
await fs.promises.cp(src, dest, mustNotMutateObjectDeep({ recursive: true }));
await assert.rejects(
  fs.promises.cp(src, dest, {
    dereference: true,
    errorOnExist: true,
    force: false,
    recursive: true,
  }),
  { code: 'ERR_FS_CP_EEXIST' }
);
