// This tests that cpSync throws if options is not object.
import '../common/index.mjs';
import assert from 'node:assert';
import { cpSync } from 'node:fs';

import tmpdir from '../common/tmpdir.js';
tmpdir.refresh();

assert.throws(
  () => cpSync('a', 'b', () => {}),
  { code: 'ERR_INVALID_ARG_TYPE' }
);
