// This tests that cpSync does not resolve relative symlinks when verbatimSymlinks is true.
import { mustNotMutateObjectDeep } from '../common/index.mjs';
import { nextdir } from '../common/fs.js';
import assert from 'node:assert';
import { cpSync, mkdirSync, writeFileSync, symlinkSync, readlinkSync } from 'node:fs';
import { join } from 'node:path';

import tmpdir from '../common/tmpdir.js';
tmpdir.refresh();

const src = nextdir();
mkdirSync(src, mustNotMutateObjectDeep({ recursive: true }));
writeFileSync(join(src, 'foo.js'), 'foo', 'utf8');
symlinkSync('foo.js', join(src, 'bar.js'));

const dest = nextdir();
mkdirSync(dest, mustNotMutateObjectDeep({ recursive: true }));

cpSync(src, dest, mustNotMutateObjectDeep({ recursive: true, verbatimSymlinks: true }));
const link = readlinkSync(join(dest, 'bar.js'));
assert.strictEqual(link, 'foo.js');
