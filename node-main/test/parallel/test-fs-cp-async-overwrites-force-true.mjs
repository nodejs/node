// This tests that it overwrites existing files if force is true.

import { mustCall, mustNotMutateObjectDeep } from '../common/index.mjs';
import assert from 'node:assert';
import { cp, mkdirSync, readFileSync, writeFileSync } from 'node:fs';
import { join } from 'node:path';
import tmpdir from '../common/tmpdir.js';
import { assertDirEquivalent, nextdir } from '../common/fs.js';
import fixtures from '../common/fixtures.js';

tmpdir.refresh();

const src = fixtures.path('copy/kitchen-sink');
const dest = nextdir();
mkdirSync(dest, mustNotMutateObjectDeep({ recursive: true }));
writeFileSync(join(dest, 'README.md'), '# Goodbye', 'utf8');

cp(src, dest, mustNotMutateObjectDeep({ recursive: true }), mustCall((err) => {
  assert.strictEqual(err, null);
  assertDirEquivalent(src, dest);
  const content = readFileSync(join(dest, 'README.md'), 'utf8');
  assert.strictEqual(content.trim(), '# Hello');
}));
