// This tests that fs.promises.cp() rejects if options is not object.

import '../common/index.mjs';
import assert from 'node:assert';
import fs from 'node:fs';
import tmpdir from '../common/tmpdir.js';

tmpdir.refresh();

await assert.rejects(
  fs.promises.cp('a', 'b', () => {}),
  { code: 'ERR_INVALID_ARG_TYPE' }
);
