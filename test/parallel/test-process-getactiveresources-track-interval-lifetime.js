'use strict';

const common = require('../common');

const assert = require('assert');

assert.strictEqual(process.getActiveResourcesInfo().filter(
  (type) => type === 'Timeout').length, 0);

let count = 0;
const interval = setInterval(common.mustCall(() => {
  assert.strictEqual(process.getActiveResourcesInfo().filter(
    (type) => type === 'Timeout').length, 1);
  ++count;
  if (count === 3) {
    clearInterval(interval);
  }
}, 3), 0);

assert.strictEqual(process.getActiveResourcesInfo().filter(
  (type) => type === 'Timeout').length, 1);
