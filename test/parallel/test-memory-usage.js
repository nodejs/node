'use strict';
require('../common');
const assert = require('assert');

const r = process.memoryUsage();
assert.ok(r.rss > 0);
assert.ok(r.heapTotal > 0);
assert.ok(r.heapUsed > 0);
assert.ok(r.external > 0);
