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

const invalidArgValueError =
  common.expectsError({ code: 'ERR_INVALID_ARG_VALUE', type: TypeError }, 13);

const invalidArgTypeError =
  common.expectsError({ code: 'ERR_INVALID_ARG_TYPE', type: TypeError }, 11);

assert.throws(function() {
  const child = spawn(invalidcmd, 'this is not an array');
  child.on('error', common.mustNotCall());
}, TypeError);

// verify that valid argument combinations do not throw
assert.doesNotThrow(function() {
  spawn(cmd);
});

assert.doesNotThrow(function() {
  spawn(cmd, []);
});

assert.doesNotThrow(function() {
  spawn(cmd, {});
});

assert.doesNotThrow(function() {
  spawn(cmd, [], {});
});

// verify that invalid argument combinations throw
assert.throws(function() {
  spawn();
}, invalidArgTypeError);

assert.throws(function() {
  spawn('');
}, invalidArgTypeError);

assert.throws(function() {
  const file = { toString() { return null; } };
  spawn(file);
}, invalidArgTypeError);

assert.throws(function() {
  spawn(cmd, null);
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

// Argument types for combinatorics
const a = [];
const o = {};
function c() {}
const s = 'string';
const u = undefined;
const n = null;

// function spawn(file=f [,args=a] [, options=o]) has valid combinations:
//   (f)
//   (f, a)
//   (f, a, o)
//   (f, o)
assert.doesNotThrow(function() { spawn(cmd); });
assert.doesNotThrow(function() { spawn(cmd, a); });
assert.doesNotThrow(function() { spawn(cmd, a, o); });
assert.doesNotThrow(function() { spawn(cmd, o); });

// Variants of undefined as explicit 'no argument' at a position
assert.doesNotThrow(function() { spawn(cmd, u, o); });
assert.doesNotThrow(function() { spawn(cmd, a, u); });

assert.throws(function() { spawn(cmd, n, o); }, invalidArgTypeError);
assert.throws(function() { spawn(cmd, a, n); }, invalidArgTypeError);

assert.throws(function() { spawn(cmd, s); }, invalidArgTypeError);
assert.throws(function() { spawn(cmd, a, s); }, invalidArgTypeError);


// verify that execFile has same argument parsing behavior as spawn
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
assert.doesNotThrow(function() { execFile(cmd); });
assert.doesNotThrow(function() { execFile(cmd, a); });
assert.doesNotThrow(function() { execFile(cmd, a, o); });
assert.doesNotThrow(function() { execFile(cmd, a, o, c); });
assert.doesNotThrow(function() { execFile(cmd, a, c); });
assert.doesNotThrow(function() { execFile(cmd, o); });
assert.doesNotThrow(function() { execFile(cmd, o, c); });
assert.doesNotThrow(function() { execFile(cmd, c); });

// Variants of undefined as explicit 'no argument' at a position
assert.doesNotThrow(function() { execFile(cmd, u, o, c); });
assert.doesNotThrow(function() { execFile(cmd, a, u, c); });
assert.doesNotThrow(function() { execFile(cmd, a, o, u); });
assert.doesNotThrow(function() { execFile(cmd, n, o, c); });
assert.doesNotThrow(function() { execFile(cmd, a, n, c); });
assert.doesNotThrow(function() { execFile(cmd, a, o, n); });
assert.doesNotThrow(function() { execFile(cmd, u, u, u); });
assert.doesNotThrow(function() { execFile(cmd, u, u, c); });
assert.doesNotThrow(function() { execFile(cmd, u, o, u); });
assert.doesNotThrow(function() { execFile(cmd, a, u, u); });
assert.doesNotThrow(function() { execFile(cmd, n, n, n); });
assert.doesNotThrow(function() { execFile(cmd, n, n, c); });
assert.doesNotThrow(function() { execFile(cmd, n, o, n); });
assert.doesNotThrow(function() { execFile(cmd, a, n, n); });
assert.doesNotThrow(function() { execFile(cmd, a, u); });
assert.doesNotThrow(function() { execFile(cmd, a, n); });
assert.doesNotThrow(function() { execFile(cmd, o, u); });
assert.doesNotThrow(function() { execFile(cmd, o, n); });
assert.doesNotThrow(function() { execFile(cmd, c, u); });
assert.doesNotThrow(function() { execFile(cmd, c, n); });

// string is invalid in arg position (this may seem strange, but is
// consistent across node API, cf. `net.createServer('not options', 'not
// callback')`
assert.throws(function() { execFile(cmd, s, o, c); }, invalidArgValueError);
assert.throws(function() { execFile(cmd, a, s, c); }, invalidArgValueError);
assert.throws(function() { execFile(cmd, a, o, s); }, invalidArgValueError);
assert.throws(function() { execFile(cmd, a, s); }, invalidArgValueError);
assert.throws(function() { execFile(cmd, o, s); }, invalidArgValueError);
assert.throws(function() { execFile(cmd, u, u, s); }, invalidArgValueError);
assert.throws(function() { execFile(cmd, n, n, s); }, invalidArgValueError);
assert.throws(function() { execFile(cmd, a, u, s); }, invalidArgValueError);
assert.throws(function() { execFile(cmd, a, n, s); }, invalidArgValueError);
assert.throws(function() { execFile(cmd, u, o, s); }, invalidArgValueError);
assert.throws(function() { execFile(cmd, n, o, s); }, invalidArgValueError);
assert.doesNotThrow(function() { execFile(cmd, c, s); });


// verify that fork has same argument parsing behavior as spawn
//
// function fork(file=f [,args=a] [, options=o]) has valid combinations:
//   (f)
//   (f, a)
//   (f, a, o)
//   (f, o)
assert.doesNotThrow(function() { fork(empty); });
assert.doesNotThrow(function() { fork(empty, a); });
assert.doesNotThrow(function() { fork(empty, a, o); });
assert.doesNotThrow(function() { fork(empty, o); });
assert.doesNotThrow(function() { fork(empty, u, u); });
assert.doesNotThrow(function() { fork(empty, u, o); });
assert.doesNotThrow(function() { fork(empty, a, u); });
assert.doesNotThrow(function() { fork(empty, n, n); });
assert.doesNotThrow(function() { fork(empty, n, o); });
assert.doesNotThrow(function() { fork(empty, a, n); });

assert.throws(function() { fork(empty, s); }, invalidArgValueError);
assert.throws(function() { fork(empty, a, s); }, invalidArgValueError);
