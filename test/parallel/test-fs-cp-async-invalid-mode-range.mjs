// This tests that cp() throws if mode is out of range.

import '../common/index.mjs';
import assert from 'node:assert';
import { cp } from 'node:fs';
import tmpdir from '../common/tmpdir.js';

tmpdir.refresh();

assert.throws(
  () => cp('a', 'b', { mode: -1 }, () => {}),
  { code: 'ERR_OUT_OF_RANGE' }
);
