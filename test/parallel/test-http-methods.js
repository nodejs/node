'use strict';
require('../common');
const assert = require('assert');
const http = require('http');
const util = require('util');

assert(Array.isArray(http.METHODS));
assert(http.METHODS.length > 0);
assert(http.METHODS.includes('GET'));
assert(http.METHODS.includes('HEAD'));
assert(http.METHODS.includes('POST'));
assert.deepStrictEqual(util._extend([], http.METHODS), http.METHODS.sort());
