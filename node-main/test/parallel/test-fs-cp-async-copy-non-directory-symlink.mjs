// This tests that cp() copies link if it does not point to folder in src.

import { mustCall, mustNotMutateObjectDeep } from '../common/index.mjs';
import { nextdir } from '../common/fs.js';
import assert from 'node:assert';
import { cp, mkdirSync, readlinkSync, symlinkSync } from 'node:fs';
import { join } from 'node:path';
import tmpdir from '../common/tmpdir.js';

tmpdir.refresh();

const src = nextdir();
mkdirSync(join(src, 'a', 'b'), mustNotMutateObjectDeep({ recursive: true }));
symlinkSync(src, join(src, 'a', 'c'));
const dest = nextdir();
mkdirSync(join(dest, 'a'), mustNotMutateObjectDeep({ recursive: true }));
symlinkSync(dest, join(dest, 'a', 'c'));
cp(src, dest, mustNotMutateObjectDeep({ recursive: true }), mustCall((err) => {
  assert.strictEqual(err, null);
  const link = readlinkSync(join(dest, 'a', 'c'));
  assert.strictEqual(link, src);
}));
