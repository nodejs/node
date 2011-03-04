var assert = require('assert');

assert.equal(process.uptime(), 0);

setTimeout(function() {
  var uptime = process.uptime();
  // some wiggle room to account for timer
  // granularity, processor speed, and scheduling
  assert.ok(uptime >= 2);
  assert.ok(uptime <= 3);
}, 2000);
