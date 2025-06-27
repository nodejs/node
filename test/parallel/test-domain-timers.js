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

'use strict';
const common = require('../common');
const domain = require('domain');
const assert = require('assert');

const timeoutd = domain.create();

timeoutd.on('error', common.mustCall(function(e) {
  assert.strictEqual(e.message, 'Timeout UNREFd');
}, 2));

let t;
timeoutd.run(function() {
  setTimeout(function() {
    throw new Error('Timeout UNREFd');
  }, 0).unref();

  t = setTimeout(function() {
    clearTimeout(timeout);
    throw new Error('Timeout UNREFd');
  }, 0);
});
t.unref();

const immediated = domain.create();

immediated.on('error', common.mustCall(function(e) {
  assert.strictEqual(e.message, 'Immediate Error');
}));

immediated.run(function() {
  setImmediate(function() {
    throw new Error('Immediate Error');
  });
});

const timeout = setTimeout(common.mustNotCall(), 10 * 1000);
