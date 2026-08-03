// This tests that fs.promises.cp() rejects if options.mode is invalid.

import '../common/index.mjs';
import assert from 'node:assert';
import fs from 'node:fs';
import tmpdir from '../common/tmpdir.js';

tmpdir.refresh();

await assert.rejects(
  fs.promises.cp('a', 'b', {
    mode: -1,
  }),
  { code: 'ERR_OUT_OF_RANGE' }
);
