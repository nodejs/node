'use strict';
require('../common');
var assert = require('assert');

console.error(process.uptime());
assert.ok(process.uptime() <= 2);

var original = process.uptime();

setTimeout(function() {
  var uptime = process.uptime();
  // some wiggle room to account for timer
  // granularity, processor speed, and scheduling
  assert.ok(uptime >= original + 2);
  assert.ok(uptime <= original + 3);
}, 2000);
