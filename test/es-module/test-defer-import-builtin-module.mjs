// Flags: --js-defer-import-eval --experimental-import-text

// Test that uses import.defer for a builtin module. Currently
// defer importing of a synthetic module should be a no-op
// in Node.js, so the test is mostly a smoke test that Node
// doesn't crash.

import '../common/index.mjs';
import * as assert from 'assert';

// Import the file system builtin module.
import defer * as fs from 'node:fs';

// Check that the imported module contains some known properties.
assert.notStrictEqual(fs.constants, undefined);
assert.notStrictEqual(fs.access, undefined);
assert.strictEqual(typeof fs.access, 'function');

// Check that the builtin module also exports some
// usable contructor functions.
assert.strictEqual(typeof fs.Stats, 'function');
assert.strictEqual(typeof new fs.Stats(), 'object');
