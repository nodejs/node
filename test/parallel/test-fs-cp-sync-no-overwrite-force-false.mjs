// This tests that cpSync does not throw errors when directory is copied over and force is false.
import { mustNotMutateObjectDeep } from '../common/index.mjs';
import { nextdir, assertDirEquivalent } from '../common/fs.js';
import assert from 'node:assert';
import { cpSync, mkdirSync, writeFileSync, lstatSync } from 'node:fs';
import { join } from 'node:path';

import tmpdir from '../common/tmpdir.js';
tmpdir.refresh();

const src = nextdir();
mkdirSync(join(src, 'a', 'b'), mustNotMutateObjectDeep({ recursive: true }));
writeFileSync(join(src, 'README.md'), 'hello world', 'utf8');
const dest = nextdir();
cpSync(src, dest, mustNotMutateObjectDeep({ recursive: true }));
const initialStat = lstatSync(join(dest, 'README.md'));
cpSync(src, dest, mustNotMutateObjectDeep({ force: false, recursive: true }));
// File should not have been copied over, so access times will be identical:
assertDirEquivalent(src, dest);
const finalStat = lstatSync(join(dest, 'README.md'));
assert.strictEqual(finalStat.ctime.getTime(), initialStat.ctime.getTime());
