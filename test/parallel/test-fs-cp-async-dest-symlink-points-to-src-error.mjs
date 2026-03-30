// This tests that cp() returns error if parent directory of symlink in dest points to src.

import { mustCall, mustNotMutateObjectDeep } from '../common/index.mjs';
import { nextdir } from '../common/fs.js';
import assert from 'node:assert';
import { cp, mkdirSync, symlinkSync } from 'node:fs';
import { join } from 'node:path';
import tmpdir from '../common/tmpdir.js';

tmpdir.refresh();

const src = nextdir();
mkdirSync(join(src, 'a'), mustNotMutateObjectDeep({ recursive: true }));
const dest = nextdir();
// Create symlink in dest pointing to src.
const destLink = join(dest, 'b');
mkdirSync(dest, mustNotMutateObjectDeep({ recursive: true }));
symlinkSync(src, destLink);
cp(src, join(dest, 'b', 'c'), mustCall((err) => {
  assert.strictEqual(err.code, 'ERR_FS_CP_EINVAL');
}));
