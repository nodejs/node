'use strict';
var common = require('../common');
var assert = require('assert');

var interval_fired = false,
    timeout_fired = false,
    unref_interval = false,
    unref_timer = false,
    unref_callbacks = 0,
    interval, check_unref, checks = 0;

var LONG_TIME = 10 * 1000;
var SHORT_TIME = 100;

setInterval(function() {
  interval_fired = true;
}, LONG_TIME).unref();

setTimeout(function() {
  timeout_fired = true;
}, LONG_TIME).unref();

interval = setInterval(function() {
  unref_interval = true;
  clearInterval(interval);
}, SHORT_TIME);
interval.unref();

setTimeout(function() {
  unref_timer = true;
}, SHORT_TIME).unref();

check_unref = setInterval(function() {
  if (checks > 5 || (unref_interval && unref_timer))
    clearInterval(check_unref);
  checks += 1;
}, 100);

setTimeout(function() {
  unref_callbacks++;
  this.unref();
}, SHORT_TIME);

// Should not timeout the test
setInterval(function() {
  this.unref();
}, SHORT_TIME);

// Should not assert on args.Holder()->InternalFieldCount() > 0. See #4261.
(function() {
  var t = setInterval(function() {}, 1);
  process.nextTick(t.unref.bind({}));
  process.nextTick(t.unref.bind(t));
})();

process.on('exit', function() {
  assert.strictEqual(interval_fired, false,
                     'Interval should not fire');
  assert.strictEqual(timeout_fired, false,
                     'Timeout should not fire');
  assert.strictEqual(unref_timer, true,
                     'An unrefd timeout should still fire');
  assert.strictEqual(unref_interval, true,
                     'An unrefd interval should still fire');
  assert.strictEqual(unref_callbacks, 1,
                     'Callback should only run once');
});
