// This tests that cpSync throws EEXIST error if attempt is made to copy symlink over file.
import { mustNotMutateObjectDeep } from '../common/index.mjs';
import { nextdir } from '../common/fs.js';
import assert from 'node:assert';
import { cpSync, mkdirSync, symlinkSync, writeFileSync } from 'node:fs';
import { join } from 'node:path';

import tmpdir from '../common/tmpdir.js';
tmpdir.refresh();

const src = nextdir();
mkdirSync(join(src, 'a', 'b'), mustNotMutateObjectDeep({ recursive: true }));
symlinkSync(join(src, 'a', 'b'), join(src, 'a', 'c'));

const dest = nextdir();
mkdirSync(join(dest, 'a'), mustNotMutateObjectDeep({ recursive: true }));
writeFileSync(join(dest, 'a', 'c'), 'hello', 'utf8');
assert.throws(
  () => cpSync(src, dest, mustNotMutateObjectDeep({ recursive: true })),
  { code: 'EEXIST' }
);
