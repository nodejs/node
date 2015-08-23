'use strict';
const common = require('../common');
const assert = require('assert');

const fs = require('fs');

const files = [];

while (files.length < 256)
  files.push(fs.openSync(__filename, 'r'));

const r = process.memoryUsage();
console.log(common.inspect(r));
assert.equal(true, r['rss'] > 0);
