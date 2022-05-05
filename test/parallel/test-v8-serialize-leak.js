'use strict';
// Flags: --expose-gc

require('../common');
const v8 = require('v8');
const assert = require('assert');

const before = process.memoryUsage.rss();

for (let i = 0; i < 1000000; i++) {
  v8.serialize('');
}

global.gc();

const after = process.memoryUsage.rss();

if (process.config.variables.asan) {
  assert(after < before * 10, `asan: before=${before} after=${after}`);
} else {
  assert(after < before * 2, `before=${before} after=${after}`);
}
