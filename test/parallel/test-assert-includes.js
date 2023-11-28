'use strict';

require('../common');
const assert = require('assert');

// Test that assert.includes works as expected

assert.doesNotThrow(() => assert.includes('I will pass', 'pass'));
assert.throws(() => assert.includes('I will fail', 'pass'));
assert.throws(() => assert.includes('fail', 'I will fail'));
assert.throws(() => assert.includes('I will fail', /fail/));
assert.throws(() => assert.includes(123, 'I will fail'));
