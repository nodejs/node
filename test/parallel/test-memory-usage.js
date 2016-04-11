'use strict';
require('../common');
var assert = require('assert');

var r = process.memoryUsage();
assert.ok(r.rss > 0);
assert.ok(r.heapTotal > 0);
assert.ok(r.heapUsed > 0);
