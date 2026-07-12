'use strict';

// Monkeypatch Object.keys() so that it throws an unexpected error. This tests
// that `util.inspect()` is unaffected by monkey-patching `Object`.

require('../common');
const assert = require('assert');
const util = require('util');

Object.keys = () => { throw new Error('fhqwhgads'); };
assert.strictEqual(util.inspect({}), '{}');
