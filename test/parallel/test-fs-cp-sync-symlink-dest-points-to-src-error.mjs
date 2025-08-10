// This tests that cpSync throws error if symlink in dest points to location in src.
import { mustNotMutateObjectDeep } from '../common/index.mjs';
import { nextdir } from '../common/fs.js';
import assert from 'node:assert';
import { cpSync, mkdirSync, symlinkSync } from 'node:fs';
import { join } from 'node:path';

import tmpdir from '../common/tmpdir.js';
tmpdir.refresh();

const src = nextdir();
mkdirSync(join(src, 'a', 'b'), mustNotMutateObjectDeep({ recursive: true }));
symlinkSync(join(src, 'a', 'b'), join(src, 'a', 'c'));

const dest = nextdir();
mkdirSync(join(dest, 'a'), mustNotMutateObjectDeep({ recursive: true }));
symlinkSync(src, join(dest, 'a', 'c'));
assert.throws(
  () => cpSync(src, dest, mustNotMutateObjectDeep({ recursive: true })),
  { code: 'ERR_FS_CP_SYMLINK_TO_SUBDIRECTORY' }
);
