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

var pass;
var wait = setInterval(function(){}, 1000);

process.on('exit', function(code) {
  if (code === 0) {
    assert.ok(pass);
  }
});

// test variants of pid
//
// null: TypeError
// undefined: TypeError
//
// 'SIGTERM': TypeError
//
// String(process.pid): TypeError
//
// Nan, Infinity, -Infinity: TypeError
//
// 0: our group process
//
// process.pid: ourself

assert.throws(function() { process.kill('SIGTERM'); }, TypeError);
assert.throws(function() { process.kill(String(process.pid)); }, TypeError);
assert.throws(function() { process.kill(null); }, TypeError);
assert.throws(function() { process.kill(undefined); }, TypeError);
assert.throws(function() { process.kill(+'not a number'); }, TypeError);
assert.throws(function() { process.kill(1/0); }, TypeError);
assert.throws(function() { process.kill(-1/0); }, TypeError);

/* Sending SIGHUP is not supported on Windows */
if (process.platform === 'win32') {
  pass = true;
  clearInterval(wait);
  return;
}

process.once('SIGHUP', function() {
  process.once('SIGHUP', function() {
    pass = true;
    clearInterval(wait);
  });
  process.kill(process.pid, 'SIGHUP');
});


/*
 * Monkey patch _kill so that, in the specific case
 * when we want to test sending a signal to pid 0,
 * we don't actually send the signal to the process group.
 * Otherwise, it could cause a lot of trouble, like terminating
 * the test runner, or any other process that belongs to the
 * same process group as this test.
 */
var origKill = process._kill;
process._kill = function(pid, sig) {
    /*
     * make sure we patch _kill only when
     * we want to test sending a signal
     * to the process group.
     */
    assert.strictEqual(pid, 0);

    // make sure that _kill is passed the correct args types
    assert(typeof pid === 'number');
    assert(typeof sig === 'number');

    // un-monkey patch process._kill
    process._kill = origKill;
    process._kill(process.pid, sig);
}

process.kill(0, 'SIGHUP');
