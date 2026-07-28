// This tests that fs.promises.cp() accepts file URL as src and dest.

import '../common/index.mjs';
import assert from 'node:assert';
import { promises as fs } from 'node:fs';
import { pathToFileURL } from 'node:url';
import { assertDirEquivalent, nextdir } from '../common/fs.js';
import tmpdir from '../common/tmpdir.js';
import fixtures from '../common/fixtures.js';

tmpdir.refresh();

const src = fixtures.path('copy/kitchen-sink');
const dest = nextdir();
const p = await fs.cp(
  pathToFileURL(src),
  pathToFileURL(dest),
  { recursive: true }
);
assert.strictEqual(p, undefined);
assertDirEquivalent(src, dest);
