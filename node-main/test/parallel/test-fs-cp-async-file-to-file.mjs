// This tests that cp() allows file to be copied to a file path.

import { mustCall, mustNotMutateObjectDeep } from '../common/index.mjs';
import { nextdir } from '../common/fs.js';
import assert from 'node:assert';
import { cp, lstatSync } from 'node:fs';
import { join } from 'node:path';
import tmpdir from '../common/tmpdir.js';
import fixtures from '../common/fixtures.js';

tmpdir.refresh();

const srcFile = fixtures.path('copy/kitchen-sink/README.md');
const destFile = join(nextdir(), 'index.js');
cp(srcFile, destFile, mustNotMutateObjectDeep({ dereference: true }), mustCall((err) => {
  assert.strictEqual(err, null);
  const stat = lstatSync(destFile);
  assert(stat.isFile());
}));
