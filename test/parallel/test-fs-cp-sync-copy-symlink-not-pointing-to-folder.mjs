// This tests that cpSync copies link if it does not point to folder in src.
import { mustNotMutateObjectDeep } from '../common/index.mjs';
import { nextdir } from '../common/fs.js';
import assert from 'node:assert';
import { cpSync, mkdirSync, symlinkSync, readlinkSync } from 'node:fs';
import { join } from 'node:path';

import tmpdir from '../common/tmpdir.js';
tmpdir.refresh();

const src = nextdir();
mkdirSync(join(src, 'a', 'b'), mustNotMutateObjectDeep({ recursive: true }));
symlinkSync(src, join(src, 'a', 'c'));
const dest = nextdir();
mkdirSync(join(dest, 'a'), mustNotMutateObjectDeep({ recursive: true }));
symlinkSync(dest, join(dest, 'a', 'c'));
cpSync(src, dest, mustNotMutateObjectDeep({ recursive: true }));
const link = readlinkSync(join(dest, 'a', 'c'));
assert.strictEqual(link, src);
