'use strict';
require('../common');
const assert = require('assert');

const fs = require('fs');

const files = [];

while (files.length < 256)
  files.push(fs.openSync(__filename, 'r'));

const r = process.memoryUsage();
assert.strictEqual(r.rss > 0, true);
