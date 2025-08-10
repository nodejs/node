// This tests that cp() throws if options is not object.

import '../common/index.mjs';
import assert from 'node:assert';
import { cp } from 'node:fs';
import tmpdir from '../common/tmpdir.js';

tmpdir.refresh();

assert.throws(
  () => cp('a', 'b', 'hello', () => {}),
  { code: 'ERR_INVALID_ARG_TYPE' }
);
