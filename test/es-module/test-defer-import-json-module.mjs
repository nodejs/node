// Flags: --js-defer-import-eval

// Test that uses import.defer for a JSON module. Currently
// defer importing of a synthetic module should be a no-op
// in Node.js, so the test is mostly a smoke test that Node
// doesn't crash.

import '../common/index.mjs';
import * as assert from 'assert';

import defer * as imported_json
  from '../fixtures/json-with-directory-name-module/module-stub.json'
  with { type: 'json' };

// Check that the imported object has the expected key/value.
assert.strictEqual(imported_json.default.rocko, 'artischocko');
