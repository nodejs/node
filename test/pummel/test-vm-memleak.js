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
// Flags: --max_old_space_size=32

require('../common');
const assert = require('assert');

const start = Date.now();
let maxMem = 0;

const ok = process.execArgv.some(function(arg) {
  return arg === '--max_old_space_size=32';
});
assert(ok, 'Run this test with --max_old_space_size=32.');

const interval = setInterval(function() {
  try {
    require('vm').runInNewContext('throw 1;');
  } catch (e) {
  }

  const rss = process.memoryUsage().rss;
  maxMem = Math.max(rss, maxMem);

  if (Date.now() - start > 5 * 1000) {
    // wait 10 seconds.
    clearInterval(interval);

    testContextLeak();
  }
}, 1);

function testContextLeak() {
  for (let i = 0; i < 1000; i++)
    require('vm').createContext({});
}

process.on('exit', function() {
  console.error('max mem: %dmb', Math.round(maxMem / (1024 * 1024)));
  assert.ok(maxMem < 64 * 1024 * 1024);
});
