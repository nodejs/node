// This tests that cpSync allows copying symlinks in src to locations in dest with
// existing symlinks not pointing to a directory.
import { mustNotMutateObjectDeep } from '../common/index.mjs';
import { nextdir } from '../common/fs.js';
import { cpSync, mkdirSync, writeFileSync, symlinkSync } from 'node:fs';
import { resolve, join } from 'node:path';

import tmpdir from '../common/tmpdir.js';
tmpdir.refresh();

const src = nextdir();
const dest = nextdir();
mkdirSync(src, mustNotMutateObjectDeep({ recursive: true }));
writeFileSync(`${src}/test.txt`, 'test');
symlinkSync(resolve(`${src}/test.txt`), join(src, 'link.txt'));
cpSync(src, dest, mustNotMutateObjectDeep({ recursive: true }));
cpSync(src, dest, mustNotMutateObjectDeep({ recursive: true }));
