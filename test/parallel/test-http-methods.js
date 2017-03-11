'use strict';
require('../common');
const assert = require('assert');
const http = require('http');
const util = require('util');

assert(Array.isArray(http.METHODS));
assert(http.METHODS.length > 0);
assert(http.METHODS.indexOf('GET') !== -1);
assert(http.METHODS.indexOf('HEAD') !== -1);
assert(http.METHODS.indexOf('POST') !== -1);
assert.deepEqual(util._extend([], http.METHODS), http.METHODS.sort());
