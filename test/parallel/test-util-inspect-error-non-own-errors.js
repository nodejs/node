'use strict';
require('../common');
const assert = require('assert');
const util = require('util');

// Test that inspecting an object with a non-own 'errors' property does not crash.
// Regression test for https://github.com/nodejs/node/issues/60808 (conceptually)

class MockError extends Error {
    constructor() {
        super('foo');
    }
    // 'errors' is a getter on the prototype, not an own property
    get errors() {
        return ['bar'];
    }
}

const err = new MockError();
assert.doesNotThrow(() => util.inspect(err));
