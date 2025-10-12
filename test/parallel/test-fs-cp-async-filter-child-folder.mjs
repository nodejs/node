// This tests that cp() should not throw exception if child folder is filtered out.

import { mustCall, mustNotMutateObjectDeep } from '../common/index.mjs';
import { nextdir } from '../common/fs.js';
import assert from 'node:assert';
import { cp, cpSync, mkdirSync, writeFileSync } from 'node:fs';
import { join } from 'node:path';
import tmpdir from '../common/tmpdir.js';

tmpdir.refresh();

const src = nextdir();
mkdirSync(join(src, 'test-cp'), mustNotMutateObjectDeep({ recursive: true }));

const dest = nextdir();
mkdirSync(dest, mustNotMutateObjectDeep({ recursive: true }));
writeFileSync(join(dest, 'test-cp'), 'test-content', mustNotMutateObjectDeep({ mode: 0o444 }));

const opts = {
  filter: (path) => !path.includes('test-cp'),
  recursive: true,
};
cp(src, dest, opts, mustCall((err) => {
  assert.strictEqual(err, null);
}));
cpSync(src, dest, opts);
