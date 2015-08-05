'use strict';
const common = require('../common');
const assert = require('assert');

var r = process.memoryUsage();
console.log(common.inspect(r));
assert.equal(true, r['rss'] > 0);
