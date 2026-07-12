// This tests that cpSync must not throw error if attempt is made to copy to dest
// directory if the parent of dest has same prefix as src directory.
import { mustNotMutateObjectDeep } from '../common/index.mjs';
import { nextdir } from '../common/fs.js';
import { cpSync, mkdirSync } from 'node:fs';

import tmpdir from '../common/tmpdir.js';
tmpdir.refresh();

const src = nextdir('aa', tmpdir);
const destParent = nextdir('aaa', tmpdir);
const dest = nextdir('aaa/aabb', tmpdir);
mkdirSync(src);
mkdirSync(destParent);
mkdirSync(dest);
cpSync(src, dest, mustNotMutateObjectDeep({ recursive: true }));
