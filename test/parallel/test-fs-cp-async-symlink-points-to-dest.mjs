// This tests that cp() returns error if symlink in src points to location in dest.

import { mustCall, mustNotMutateObjectDeep } from '../common/index.mjs';
import { nextdir } from '../common/fs.js';
import assert from 'node:assert';
import { cp, cpSync, mkdirSync, symlinkSync } from 'node:fs';
import { join } from 'node:path';
import tmpdir from '../common/tmpdir.js';

tmpdir.refresh();

const src = nextdir();
mkdirSync(src, mustNotMutateObjectDeep({ recursive: true }));
const dest = nextdir();
mkdirSync(dest);
symlinkSync(dest, join(src, 'link'));
cpSync(src, dest, mustNotMutateObjectDeep({ recursive: true }));
cp(src, dest, mustNotMutateObjectDeep({ recursive: true }), mustCall((err) => {
  assert.strictEqual(err.code, 'ERR_FS_CP_EINVAL');
}));
