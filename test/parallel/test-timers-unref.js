'use strict';
require('../common');
var assert = require('assert');

let interval_fired = false;
let timeout_fired = false;
let unref_interval = false;
let unref_timer = false;
let unref_callbacks = 0;
let checks = 0;

var LONG_TIME = 10 * 1000;
var SHORT_TIME = 100;

assert.doesNotThrow(function() {
  setTimeout(function() {}, 10).unref().ref().unref();
}, 'ref and unref are chainable');

assert.doesNotThrow(function() {
  setInterval(function() {}, 10).unref().ref().unref();
}, 'ref and unref are chainable');

setInterval(function() {
  interval_fired = true;
}, LONG_TIME).unref();

setTimeout(function() {
  timeout_fired = true;
}, LONG_TIME).unref();

const interval = setInterval(function() {
  unref_interval = true;
  clearInterval(interval);
}, SHORT_TIME);
interval.unref();

setTimeout(function() {
  unref_timer = true;
}, SHORT_TIME).unref();

const check_unref = setInterval(function() {
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
{
  const t = setInterval(function() {}, 1);
  process.nextTick(t.unref.bind({}));
  process.nextTick(t.unref.bind(t));
}

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
