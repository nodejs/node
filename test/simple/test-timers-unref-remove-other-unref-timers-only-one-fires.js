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

/*
 * The goal of this test is to make sure that, after the regression introduced
 * by 934bfe23a16556d05bfb1844ef4d53e8c9887c3d, the fix preserves the following
 * behavior of unref timers: if two timers are scheduled to fire at the same
 * time, if one unenrolls the other one in its _onTimeout callback, the other
 * one will *not* fire.
 *
 * This behavior is a private implementation detail and should not be
 * considered public interface.
 */
var timers = require('timers');
var assert = require('assert');

var nbTimersFired = 0;

var foo = new function() {
  this._onTimeout = function() {
    ++nbTimersFired;
    timers.unenroll(bar);
  };
}();

var bar = new function() {
  this._onTimeout = function() {
    ++nbTimersFired;
    timers.unenroll(foo);
  };
}();

timers.enroll(bar, 1);
timers._unrefActive(bar);

timers.enroll(foo, 1);
timers._unrefActive(foo);

setTimeout(function() {
  assert.notEqual(nbTimersFired, 2);
}, 20);
