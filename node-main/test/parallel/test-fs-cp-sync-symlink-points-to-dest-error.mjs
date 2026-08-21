// This tests that cpSync throws error if symlink in src points to location in dest.
import { mustNotMutateObjectDeep } from '../common/index.mjs';
import { nextdir } from '../common/fs.js';
import assert from 'node:assert';
import { cpSync, mkdirSync, symlinkSync } from 'node:fs';
import { join } from 'node:path';

import tmpdir from '../common/tmpdir.js';
tmpdir.refresh();

const src = nextdir();
mkdirSync(src, mustNotMutateObjectDeep({ recursive: true }));
const dest = nextdir();
mkdirSync(dest);
symlinkSync(dest, join(src, 'link'));
cpSync(src, dest, mustNotMutateObjectDeep({ recursive: true }));
assert.throws(
  () => cpSync(src, dest, mustNotMutateObjectDeep({ recursive: true })),
  {
    code: 'ERR_FS_CP_EINVAL'
  }
);
