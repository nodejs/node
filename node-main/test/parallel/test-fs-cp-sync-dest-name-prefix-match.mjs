// This tests that cpSync must not throw error if attempt is made to copy to dest
// directory with same prefix as src directory.
import { mustNotMutateObjectDeep } from '../common/index.mjs';
import { nextdir } from '../common/fs.js';
import { cpSync, mkdirSync } from 'node:fs';

import tmpdir from '../common/tmpdir.js';
tmpdir.refresh();

const src = nextdir('prefix', tmpdir);
const dest = nextdir('prefix-a', tmpdir);
mkdirSync(src);
mkdirSync(dest);
cpSync(src, dest, mustNotMutateObjectDeep({ recursive: true }));
