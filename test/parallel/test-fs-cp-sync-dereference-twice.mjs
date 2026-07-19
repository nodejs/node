// This tests that cpSync does not fail if the same directory is copied to dest twice,
// when dereference is true, and force is false (fails silently).
import { mustNotMutateObjectDeep } from '../common/index.mjs';
import { nextdir } from '../common/fs.js';
import assert from 'assert';

import { cpSync, lstatSync } from 'node:fs';
import { join } from 'node:path';
import tmpdir from '../common/tmpdir.js';
import fixtures from '../common/fixtures.js';

tmpdir.refresh();

const src = fixtures.path('copy/kitchen-sink');
const dest = nextdir();
const destFile = join(dest, 'a/b/README2.md');
cpSync(src, dest, mustNotMutateObjectDeep({ dereference: true, recursive: true }));
cpSync(src, dest, mustNotMutateObjectDeep({ dereference: true, recursive: true }));
const stat = lstatSync(destFile);
assert(stat.isFile());
