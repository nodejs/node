// This tests that cpSync throws error when verbatimSymlinks is not a boolean.
import '../common/index.mjs';
import assert from 'node:assert';
import { cpSync } from 'node:fs';
import tmpdir from '../common/tmpdir.js';
import fixtures from '../common/fixtures.js';

tmpdir.refresh();

const src = fixtures.path('copy/kitchen-sink');
[1, [], {}, null, 1n, undefined, null, Symbol(), '', () => {}]
  .forEach((verbatimSymlinks) => {
    assert.throws(
      () => cpSync(src, src, { verbatimSymlinks }),
      { code: 'ERR_INVALID_ARG_TYPE' }
    );
  });
