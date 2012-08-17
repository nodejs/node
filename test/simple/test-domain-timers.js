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

var domain = require('domain');
var assert = require('assert');
var common = require('../common.js');

var timeout_err, timeout, immediate_err;

var timeoutd = domain.create();

timeoutd.on('error', function(e) {
  timeout_err = e;
  clearTimeout(timeout);
});

timeoutd.run(function() {
  setTimeout(function() {
    throw new Error('Timeout UNREFd');
  }).unref();
});

var immediated = domain.create();

immediated.on('error', function(e) {
  immediate_err = e;
});

immediated.run(function() {
  setImmediate(function() {
    throw new Error('Immediate Error');
  });
});

timeout = setTimeout(function() {}, 10 * 1000);

process.on('exit', function() {
  assert.equal(timeout_err.message, 'Timeout UNREFd',
      'Domain should catch timer error');
  assert.equal(immediate_err.message, 'Immediate Error',
      'Domain should catch immediate error');
});
