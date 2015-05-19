'use strict';
var common = require('../common');
var assert = require('assert');

var r = process.memoryUsage();
console.log(common.inspect(r));
assert.equal(true, r['rss'] > 0);
