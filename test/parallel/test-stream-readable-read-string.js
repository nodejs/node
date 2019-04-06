'use strict';
require('../common');
const assert = require('assert');
const { Readable } = require('stream');

// This test verifies that we coerce input to `.read()` to Number according to
// the typical JS rules, and not using `parseInt()`. Passing strings to
// `.read()` is not part of the public API, so in a way this is purely an
// internals test.

const readable = new Readable({ read() {} });
readable.push('a'.repeat(100));
assert.strictEqual(readable.read('1e2').length, 100);
