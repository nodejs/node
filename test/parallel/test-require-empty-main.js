'use strict';
require('../common');

// A package.json with an empty "main" property should use index.js if present.
// require.resolve() should resolve to index.js for the same reason.
//
// In fact, any "main" property that doesn't resolve to a file should result
// in index.js being used, but that's already checked for by other tests.
// This test only concerns itself with the empty string.

const assert = require('assert');
const path = require('path');
const fixtures = require('../common/fixtures');

const where = fixtures.path('require-empty-main');
const expected = path.join(where, 'index.js');

test();
setImmediate(test);

function test() {
  assert.strictEqual(require.resolve(where), expected);
  assert.strictEqual(require(where), 42);
  assert.strictEqual(require.resolve(where), expected);
}
