// This tests that cp() should not throw exception if dest is invalid but filtered out.

import { mustCall, mustNotMutateObjectDeep } from '../common/index.mjs';
import { nextdir } from '../common/fs.js';
import assert from 'node:assert';
import { cp, cpSync, mkdirSync, writeFileSync } from 'node:fs';
import { join } from 'node:path';
import tmpdir from '../common/tmpdir.js';

tmpdir.refresh();

// Create dest as a file.
// Expect: cp skips the copy logic entirely and won't throw any exception in path validation process.
const src = join(nextdir(), 'bar');
mkdirSync(src, mustNotMutateObjectDeep({ recursive: true }));

const destParent = nextdir();
const dest = join(destParent, 'bar');
mkdirSync(destParent, mustNotMutateObjectDeep({ recursive: true }));
writeFileSync(dest, 'test-content', mustNotMutateObjectDeep({ mode: 0o444 }));

const opts = {
  filter: (path) => !path.includes('bar'),
  recursive: true,
};
cp(src, dest, opts, mustCall((err) => {
  assert.strictEqual(err, null);
}));
cpSync(src, dest, opts);
