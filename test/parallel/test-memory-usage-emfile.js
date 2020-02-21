'use strict';
const common = require('../common');

// On IBMi, the rss memory always returns zero
if (common.isIBMi)
  common.skip('On IBMi, the rss memory always returns zero');

const assert = require('assert');

const fs = require('fs');

const files = [];

while (files.length < 256)
  files.push(fs.openSync(__filename, 'r'));

const r = process.memoryUsage();
assert.strictEqual(r.rss > 0, true);
