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
var tracing = require('tracing');

// If there is an uncaughtException listener then the error thrown from
// "before" will be considered handled, thus calling setImmediate to
// finish execution of the nextTickQueue. This in turn will cause "before"
// to fire again, entering into an infinite loop.
// So the asyncQueue is cleared from the returned setImmediate in
// _fatalException to prevent this from happening.
var cntr = 0;


tracing.addAsyncListener({
  before: function() {
    if (++cntr > 1) {
      // Can't throw since uncaughtException will also catch that.
      process._rawDebug('Error: Multiple before callbacks called');
      process.exit(1);
    }
    throw new Error('before');
  }
});

process.on('uncaughtException', function() { });

process.nextTick();
