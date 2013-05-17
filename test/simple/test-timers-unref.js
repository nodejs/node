// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

var common = require('../common');
var assert = require('assert');

var interval_fired = false,
    timeout_fired = false,
    unref_interval = false,
    unref_timer = false,
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
}, SHORT_TIME).unref();

setTimeout(function() {
  unref_timer = true;
}, SHORT_TIME).unref();

check_unref = setInterval(function() {
  if (checks > 5 || (unref_interval && unref_timer))
    clearInterval(check_unref);
  checks += 1;
}, 100);

// Should not assert on args.Holder()->InternalFieldCount() > 0. See #4261.
(function() {
  var t = setInterval(function() {}, 1);
  process.nextTick(t.unref.bind({}));
  process.nextTick(t.unref.bind(t));
})();

process.on('exit', function() {
  assert.strictEqual(interval_fired, false, 'Interval should not fire');
  assert.strictEqual(timeout_fired, false, 'Timeout should not fire');
  assert.strictEqual(unref_timer, true, 'An unrefd timeout should still fire');
  assert.strictEqual(unref_interval, true, 'An unrefd interval should still fire');
});
