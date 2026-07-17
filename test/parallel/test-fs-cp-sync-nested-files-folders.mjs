// This tests that cpSync copies a nested folder structure with files and folders.
import { mustNotMutateObjectDeep } from '../common/index.mjs';
import { nextdir, assertDirEquivalent } from '../common/fs.js';
import { cpSync } from 'node:fs';
import tmpdir from '../common/tmpdir.js';
import fixtures from '../common/fixtures.js';

tmpdir.refresh();

const src = fixtures.path('copy/kitchen-sink');
const dest = nextdir();
cpSync(src, dest, mustNotMutateObjectDeep({ recursive: true }));
assertDirEquivalent(src, dest);
