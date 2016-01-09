'use strict';
require('../common');
var assert = require('assert');
var http = require('http');

assert(Array.isArray(http.METHODS));
assert(http.METHODS.length > 0);
assert(http.METHODS.indexOf('GET') !== -1);
assert(http.METHODS.indexOf('HEAD') !== -1);
assert(http.METHODS.indexOf('POST') !== -1);
assert.deepEqual(Object.assign([], http.METHODS), http.METHODS.sort());
