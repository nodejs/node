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
 * This test is a regression test for joyent/node#8897.
 *
 * It tests some private implementation details that should not be
 * considered public interface.
 */
var timers = require('timers');

var foo = new function() {
  this._onTimeout = function() {};
}();

var bar = new function() {
  this._onTimeout = function() {
    timers.unenroll(foo);
  };
}();

// We use timers with expiration times that are sufficiently apart to make
// sure that they're not fired at the same time on platforms where the timer
// resolution is a bit coarse (e.g Windows with a default resolution of ~15ms).
timers.enroll(bar, 1);
timers._unrefActive(bar);

timers.enroll(foo, 50);
timers._unrefActive(foo);

setTimeout(function() {}, 100);
