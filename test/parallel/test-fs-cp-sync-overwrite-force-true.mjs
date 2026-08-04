// This tests that cpSync overwrites existing files if force is true.
import { mustNotMutateObjectDeep } from '../common/index.mjs';
import { nextdir, assertDirEquivalent } from '../common/fs.js';
import assert from 'node:assert';
import { cpSync, mkdirSync, writeFileSync, readFileSync } from 'node:fs';
import { join } from 'node:path';
import tmpdir from '../common/tmpdir.js';
import fixtures from '../common/fixtures.js';

tmpdir.refresh();

const src = fixtures.path('copy/kitchen-sink');
const dest = nextdir();
mkdirSync(dest, mustNotMutateObjectDeep({ recursive: true }));
writeFileSync(join(dest, 'README.md'), '# Goodbye', 'utf8');
cpSync(src, dest, mustNotMutateObjectDeep({ recursive: true }));
assertDirEquivalent(src, dest);
const content = readFileSync(join(dest, 'README.md'), 'utf8');
assert.strictEqual(content.trim(), '# Hello');
