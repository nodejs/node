'use strict';

const common = require('../common');
const assert = require('assert');

for (let i = 0; i < 10; ++i) {
  for (let j = 0; j < 10; ++j) {
    setTimeout(common.mustCall(), i);
  }
}

assert.strictEqual(process.getActiveResourcesInfo().filter(
  (type) => type === 'Timeout').length, 100);

for (let i = 0; i < 10; ++i) {
  setImmediate(common.mustCall());
}

assert.strictEqual(process.getActiveResourcesInfo().filter(
  (type) => type === 'Immediate').length, 10);
