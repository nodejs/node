// This tests that cpSync copies file itself, rather than symlink, when dereference is true.
import { mustNotMutateObjectDeep } from '../common/index.mjs';
import { nextdir } from '../common/fs.js';
import assert from 'assert';

import { cpSync, mkdirSync, writeFileSync, symlinkSync, lstatSync } from 'node:fs';
import { join } from 'node:path';
import tmpdir from '../common/tmpdir.js';

tmpdir.refresh();

const src = nextdir();
mkdirSync(src, mustNotMutateObjectDeep({ recursive: true }));
writeFileSync(join(src, 'foo.js'), 'foo', 'utf8');
symlinkSync(join(src, 'foo.js'), join(src, 'bar.js'));

const dest = nextdir();
mkdirSync(dest, mustNotMutateObjectDeep({ recursive: true }));
const destFile = join(dest, 'foo.js');

cpSync(join(src, 'bar.js'), destFile, mustNotMutateObjectDeep({ dereference: true, recursive: true }));
const stat = lstatSync(destFile);
assert(stat.isFile());
