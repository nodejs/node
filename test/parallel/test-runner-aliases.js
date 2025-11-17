'use strict';
require('../common');
const assert = require('node:assert');
const test = require('node:test');

assert.strictEqual(test.test, test);
assert.strictEqual(test.it, test);
assert.strictEqual(test.describe, test.suite);
