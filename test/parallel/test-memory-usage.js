'use strict';
var common = require('../common');
var assert = require('assert');

var r = process.memoryUsage();
assert.equal(true, r['rss'] > 0);
