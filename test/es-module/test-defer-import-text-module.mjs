// Flags: --js-defer-import-eval --experimental-import-text

// Test that uses import.defer for a text module. Currently
// defer importing of a synthetic module should be a no-op
// in Node.js, so the test is mostly a smoke test that Node
// doesn't crash.

import '../common/index.mjs';
import * as assert from 'assert';

import defer * as imported_text
  from '../fixtures/file-to-read-without-bom.txt'
  with { type: 'text' };

const expected_text = 'abc\ndef\nghi\n';

// Check that the imported text has the expected value.
assert.strictEqual(imported_text.default, expected_text);
