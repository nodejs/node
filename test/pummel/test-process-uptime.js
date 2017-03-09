'use strict';
require('../common');
const assert = require('assert');

console.error(process.uptime());
assert.ok(process.uptime() <= 2);

const original = process.uptime();

setTimeout(function() {
  const uptime = process.uptime();
  // some wiggle room to account for timer
  // granularity, processor speed, and scheduling
  assert.ok(uptime >= original + 2);
  assert.ok(uptime <= original + 3);
}, 2000);
