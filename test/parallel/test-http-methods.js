'use strict';
require('../common');
const assert = require('assert');
const http = require('http');
const util = require('util');

assert(Array.isArray(http.METHODS));
assert(http.METHODS.length > 0);
assert.notStrictEqual(http.METHODS.indexOf('GET'), -1);
assert.notStrictEqual(http.METHODS.indexOf('HEAD'), -1);
assert.notStrictEqual(http.METHODS.indexOf('POST'), -1);
assert.deepStrictEqual(util._extend([], http.METHODS), http.METHODS.sort());
