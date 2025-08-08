// This tests that cp() copies file itself, rather than symlink, when dereference is true.

import { mustCall, mustNotMutateObjectDeep } from '../common/index.mjs';
import { nextdir } from '../common/fs.js';
import assert from 'node:assert';
import { cp, lstatSync, mkdirSync, symlinkSync, writeFileSync } from 'node:fs';
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

cp(join(src, 'bar.js'), destFile, mustNotMutateObjectDeep({ dereference: true }),
   mustCall((err) => {
     assert.strictEqual(err, null);
     const stat = lstatSync(destFile);
     assert(stat.isFile());
   })
);
