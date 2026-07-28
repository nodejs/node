// This tests that fs.promises.cp() copies a nested folder structure with files and folders.

import { mustNotMutateObjectDeep } from '../common/index.mjs';
import { nextdir, assertDirEquivalent } from '../common/fs.js';
import assert from 'node:assert';
import fs from 'node:fs';
import tmpdir from '../common/tmpdir.js';
import fixtures from '../common/fixtures.js';

tmpdir.refresh();

const src = fixtures.path('copy/kitchen-sink');
const dest = nextdir();
const p = await fs.promises.cp(src, dest, mustNotMutateObjectDeep({ recursive: true }));
assert.strictEqual(p, undefined);
assertDirEquivalent(src, dest);
