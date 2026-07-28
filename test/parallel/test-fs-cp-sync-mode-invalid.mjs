// This tests that cpSync rejects if options.mode is invalid.
import '../common/index.mjs';
import assert from 'node:assert';
import { cpSync } from 'node:fs';

import tmpdir from '../common/tmpdir.js';
tmpdir.refresh();

assert.throws(
  () => cpSync('a', 'b', { mode: -1 }),
  { code: 'ERR_OUT_OF_RANGE' }
);
