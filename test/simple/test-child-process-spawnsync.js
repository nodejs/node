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

var spawnSync = require('child_process').spawnSync;

var TIMER = 100;
var SLEEP = 1000;

var start = Date.now();
var timeout = 0;

setTimeout(function() {
  console.log('timer fired');
  timeout = Date.now();
}, TIMER);

console.log('sleep started');
var ret = spawnSync('sleep', ['1']);
console.log('sleep exited');

process.on('exit', function() {
  assert.strictEqual(ret.status, 0);

  var delta = Date.now() - start;

  var expected_timeout = start + TIMER;
  var tdlta = timeout - expected_timeout;

  assert(delta > SLEEP);
  assert(tdlta > TIMER && tdlta < SLEEP);
});
