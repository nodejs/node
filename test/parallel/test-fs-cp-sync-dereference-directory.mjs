// This tests that cpSync overrides target directory with what symlink points to, when dereference is true.
import { mustNotMutateObjectDeep } from '../common/index.mjs';
import { nextdir, assertDirEquivalent } from '../common/fs.js';
import assert from 'node:assert';
import { cpSync, mkdirSync, writeFileSync, symlinkSync, lstatSync } from 'node:fs';
import { join } from 'node:path';
import tmpdir from '../common/tmpdir.js';

tmpdir.refresh();

const src = nextdir();
const symlink = nextdir();
const dest = nextdir();
mkdirSync(src, mustNotMutateObjectDeep({ recursive: true }));
writeFileSync(join(src, 'foo.js'), 'foo', 'utf8');
symlinkSync(src, symlink);

mkdirSync(dest, mustNotMutateObjectDeep({ recursive: true }));

cpSync(symlink, dest, mustNotMutateObjectDeep({ dereference: true, recursive: true }));
const destStat = lstatSync(dest);
assert(!destStat.isSymbolicLink());
assertDirEquivalent(src, dest);
