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
var domain = require('domain');

// Use the same timeout value so that both timers' callbacks are called during
// the same invocation of the underlying native timer's callback (listOnTimeout
// in lib/timers.js).
setTimeout(err, 50);
setTimeout(common.mustCall(secondTimer), 50);

function err() {
  var d = domain.create();
  d.on('error', handleDomainError);
  d.run(err2);

  function err2() {
    // this function doesn't exist, and throws an error as a result.
    err3();
  }

  function handleDomainError(e) {
    // In the domain's error handler, the current active domain should be the
    // domain within which the error was thrown.
    assert.equal(process.domain, d);
  }
}

function secondTimer() {
  // secondTimer was scheduled before any domain had been created, so its
  // callback should not have any active domain set when it runs.
  // Do not use assert here, as it throws errors and if a domain with an error
  // handler is active, then asserting wouldn't make the test fail.
  if (process.domain !== null) {
    console.log('process.domain should be null, but instead is:',
      process.domain);
    process.exit(1);
  }
}
