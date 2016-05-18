'use strict';
require('../common');
var assert = require('assert');
var http = require('http');
var util = require('util');

assert(Array.isArray(http.METHODS));
assert(http.METHODS.length > 0);
assert(http.METHODS.indexOf('GET') !== -1);
assert(http.METHODS.indexOf('HEAD') !== -1);
assert(http.METHODS.indexOf('POST') !== -1);
assert.deepStrictEqual(util._extend([], http.METHODS), http.METHODS.sort());
