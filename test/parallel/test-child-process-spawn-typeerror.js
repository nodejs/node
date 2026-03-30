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
const assert = require('assert');
const { spawn, fork, execFile } = require('child_process');
const fixtures = require('../common/fixtures');
const cmd = common.isWindows ? 'rundll32' : 'ls';
const invalidcmd = 'hopefully_you_dont_have_this_on_your_machine';

const empty = fixtures.path('empty.js');

const invalidArgValueError = {
  code: 'ERR_INVALID_ARG_VALUE',
  name: 'TypeError'
};

const invalidArgTypeError = {
  code: 'ERR_INVALID_ARG_TYPE',
  name: 'TypeError'
};

const invalidRangeError = { code: 'ERR_OUT_OF_RANGE', name: 'RangeError' };

assert.throws(function() {
  spawn(invalidcmd, 'this is not an array');
}, invalidArgTypeError);

// Verify that valid argument combinations do not throw.
spawn(cmd);
spawn(cmd, []);
spawn(cmd, {});
spawn(cmd, [], {});

// Verify that invalid argument combinations throw.
assert.throws(function() {
  spawn();
}, invalidArgTypeError);

assert.throws(function() {
  spawn('');
}, invalidArgValueError);

assert.throws(function() {
  const file = { toString() { return null; } };
  spawn(file);
}, invalidArgTypeError);

assert.throws(function() {
  spawn(cmd, true);
}, invalidArgTypeError);

assert.throws(function() {
  spawn(cmd, [], null);
}, invalidArgTypeError);

assert.throws(function() {
  spawn(cmd, [], 1);
}, invalidArgTypeError);

assert.throws(function() {
  spawn(cmd, [], { uid: 2 ** 63 });
}, invalidRangeError);

assert.throws(function() {
  spawn(cmd, [], { gid: 2 ** 63 });
}, invalidRangeError);

// Argument types for combinatorics.
const a = [];
const o = {};
function c() {}
const s = 'string';
const u = undefined;
const n = null;

// Function spawn(file=f [,args=a] [, options=o]) has valid combinations:
//   (f)
//   (f, a)
//   (f, a, o)
//   (f, o)
spawn(cmd);
spawn(cmd, a);
spawn(cmd, a, o);
spawn(cmd, o);

// Variants of undefined as explicit 'no argument' at a position.
spawn(cmd, u, o);
spawn(cmd, n, o);
spawn(cmd, a, u);

assert.throws(() => { spawn(cmd, a, n); }, invalidArgTypeError);
assert.throws(() => { spawn(cmd, s); }, invalidArgTypeError);
assert.throws(() => { spawn(cmd, a, s); }, invalidArgTypeError);
assert.throws(() => { spawn(cmd, a, a); }, invalidArgTypeError);


// Verify that execFile has same argument parsing behavior as spawn.
//
// function execFile(file=f [,args=a] [, options=o] [, callback=c]) has valid
// combinations:
//   (f)
//   (f, a)
//   (f, a, o)
//   (f, a, o, c)
//   (f, a, c)
//   (f, o)
//   (f, o, c)
//   (f, c)
execFile(cmd);
execFile(cmd, a);
execFile(cmd, a, o);
execFile(cmd, a, o, c);
execFile(cmd, a, c);
execFile(cmd, o);
execFile(cmd, o, c);
execFile(cmd, c);

// Variants of undefined as explicit 'no argument' at a position.
execFile(cmd, u, o, c);
execFile(cmd, a, u, c);
execFile(cmd, a, o, u);
execFile(cmd, n, o, c);
execFile(cmd, a, n, c);
execFile(cmd, a, o, n);
execFile(cmd, u, u, u);
execFile(cmd, u, u, c);
execFile(cmd, u, o, u);
execFile(cmd, a, u, u);
execFile(cmd, n, n, n);
execFile(cmd, n, n, c);
execFile(cmd, n, o, n);
execFile(cmd, a, n, n);
execFile(cmd, a, u);
execFile(cmd, a, n);
execFile(cmd, o, u);
execFile(cmd, o, n);
execFile(cmd, c, u);
execFile(cmd, c, n);

// String is invalid in arg position (this may seem strange, but is
// consistent across node API, cf. `net.createServer('not options', 'not
// callback')`.
assert.throws(() => { execFile(cmd, s, o, c); }, invalidArgTypeError);
assert.throws(() => { execFile(cmd, a, s, c); }, invalidArgTypeError);
assert.throws(() => { execFile(cmd, a, o, s); }, invalidArgTypeError);
assert.throws(() => { execFile(cmd, a, s); }, invalidArgTypeError);
assert.throws(() => { execFile(cmd, o, s); }, invalidArgTypeError);
assert.throws(() => { execFile(cmd, u, u, s); }, invalidArgTypeError);
assert.throws(() => { execFile(cmd, n, n, s); }, invalidArgTypeError);
assert.throws(() => { execFile(cmd, a, u, s); }, invalidArgTypeError);
assert.throws(() => { execFile(cmd, a, n, s); }, invalidArgTypeError);
assert.throws(() => { execFile(cmd, u, o, s); }, invalidArgTypeError);
assert.throws(() => { execFile(cmd, n, o, s); }, invalidArgTypeError);
assert.throws(() => { execFile(cmd, a, a); }, invalidArgTypeError);

execFile(cmd, c, s); // Should not throw.

// Verify that fork has same argument parsing behavior as spawn.
//
// function fork(file=f [,args=a] [, options=o]) has valid combinations:
//   (f)
//   (f, a)
//   (f, a, o)
//   (f, o)
fork(empty);
fork(empty, a);
fork(empty, a, o);
fork(empty, o);
fork(empty, u, u);
fork(empty, u, o);
fork(empty, a, u);
fork(empty, n, n);
fork(empty, n, o);
fork(empty, a, n);

assert.throws(() => { fork(empty, s); }, invalidArgTypeError);
assert.throws(() => { fork(empty, a, s); }, invalidArgTypeError);
assert.throws(() => { fork(empty, a, a); }, invalidArgTypeError);
