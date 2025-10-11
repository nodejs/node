// This tests that cp() returns error if symlink in dest points to location in src.

import { mustCall, mustNotMutateObjectDeep } from '../common/index.mjs';
import { nextdir } from '../common/fs.js';
import assert from 'node:assert';
import { cp, mkdirSync, symlinkSync } from 'node:fs';
import { join } from 'node:path';
import tmpdir from '../common/tmpdir.js';

tmpdir.refresh();

const src = nextdir();
mkdirSync(join(src, 'a', 'b'), mustNotMutateObjectDeep({ recursive: true }));
symlinkSync(join(src, 'a', 'b'), join(src, 'a', 'c'));

const dest = nextdir();
mkdirSync(join(dest, 'a'), mustNotMutateObjectDeep({ recursive: true }));
symlinkSync(src, join(dest, 'a', 'c'));
cp(src, dest, mustNotMutateObjectDeep({ recursive: true }), mustCall((err) => {
  assert.strictEqual(err.code, 'ERR_FS_CP_SYMLINK_TO_SUBDIRECTORY');
}));
