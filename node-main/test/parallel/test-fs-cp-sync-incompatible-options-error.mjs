// This tests that cpSync throws an error when both dereference and verbatimSymlinks are enabled.
import { mustNotMutateObjectDeep } from '../common/index.mjs';
import assert from 'node:assert';
import { cpSync } from 'node:fs';
import tmpdir from '../common/tmpdir.js';
import fixtures from '../common/fixtures.js';

tmpdir.refresh();

const src = fixtures.path('copy/kitchen-sink');
assert.throws(
  () => cpSync(src, src, mustNotMutateObjectDeep({ dereference: true, verbatimSymlinks: true })),
  { code: 'ERR_INCOMPATIBLE_OPTION_PAIR' }
);
