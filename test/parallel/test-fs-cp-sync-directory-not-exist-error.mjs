// This tests that cpSync throws an error when attempting to copy a dir that does not exist.
import { mustNotMutateObjectDeep } from '../common/index.mjs';
import { nextdir } from '../common/fs.js';
import assert from 'node:assert';
import { cpSync } from 'node:fs';

import tmpdir from '../common/tmpdir.js';
tmpdir.refresh();

const src = nextdir();
const dest = nextdir();
assert.throws(
  () => cpSync(src, dest, mustNotMutateObjectDeep({ recursive: true })),
  { code: 'ENOENT' }
);
