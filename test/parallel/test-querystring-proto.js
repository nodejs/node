/* eslint-disable no-proto */
'use strict';

require('../common');
const assert = require('assert');
const qs = require('querystring');

const obj = qs.parse('a=b&__proto__=1');

assert.equal(obj.a, 'b');
assert.equal(obj.__proto__, 1);
assert(typeof Object.getPrototypeOf(obj) === 'object');
assert.strictEqual(Object.getPrototypeOf(obj), Object.prototype);
