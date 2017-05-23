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
require('../common');
const vm = require('vm');

// We're testing a race condition so we just have to spin this in a loop
// for a little while and see if it breaks. The condition being tested
// is an `isolate->TerminateExecution()` reaching the main JS stack from
// the timeout watchdog.
const sandbox = { timeout: 5 };
const context = vm.createContext(sandbox);
const script = new vm.Script(
  'var d = Date.now() + timeout;while (d > Date.now());'
);
const immediate = setImmediate(function() {
  throw new Error('Detected vm race condition!');
});

// When this condition was first discovered this test would fail in 50ms
// or so. A better, but still incorrect implementation would fail after
// 100 seconds or so. If you're messing with vm timeouts you might
// consider increasing this timeout to hammer out races.
const giveUp = Date.now() + 5000;
do {
  // The loop adjusts the timeout up or down trying to hit the race
  try {
    script.runInContext(context, { timeout: 5 });
    ++sandbox.timeout;
  } catch (err) {
    --sandbox.timeout;
  }
} while (Date.now() < giveUp);

clearImmediate(immediate);
