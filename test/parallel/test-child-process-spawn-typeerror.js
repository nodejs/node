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
const child_process = require('child_process');
const spawn = child_process.spawn;
const fork = child_process.fork;
const execFile = child_process.execFile;
const cmd = common.isWindows ? 'rundll32' : 'ls';
const invalidcmd = 'hopefully_you_dont_have_this_on_your_machine';
const invalidArgsMsg = /Incorrect value of args option/;
const invalidOptionsMsg = /"options" argument must be an object/;
const invalidFileMsg =
  /^TypeError: "file" argument must be a non-empty string$/;
const empty = common.fixturesDir + '/empty.js';

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
}, invalidFileMsg);

assert.throws(function() {
  spawn('');
}, invalidFileMsg);

assert.throws(function() {
  const file = { toString() { throw new Error('foo'); } };
  spawn(file);
}, invalidFileMsg);

assert.throws(function() {
  spawn(cmd, null);
}, invalidArgsMsg);

assert.throws(function() {
  spawn(cmd, true);
}, invalidArgsMsg);

assert.throws(function() {
  spawn(cmd, [], null);
}, invalidOptionsMsg);

assert.throws(function() {
  spawn(cmd, [], 1);
}, invalidOptionsMsg);

// Argument types for combinatorics
const a = [];
const o = {};
const c = function c() {};
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

assert.throws(function() { spawn(cmd, n, o); }, TypeError);
assert.throws(function() { spawn(cmd, a, n); }, TypeError);

assert.throws(function() { spawn(cmd, s); }, TypeError);
assert.throws(function() { spawn(cmd, a, s); }, TypeError);


// verify that execFile has same argument parsing behaviour as spawn
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
assert.throws(function() { execFile(cmd, s, o, c); }, TypeError);
assert.throws(function() { execFile(cmd, a, s, c); }, TypeError);
assert.throws(function() { execFile(cmd, a, o, s); }, TypeError);
assert.throws(function() { execFile(cmd, a, s); }, TypeError);
assert.throws(function() { execFile(cmd, o, s); }, TypeError);
assert.throws(function() { execFile(cmd, u, u, s); }, TypeError);
assert.throws(function() { execFile(cmd, n, n, s); }, TypeError);
assert.throws(function() { execFile(cmd, a, u, s); }, TypeError);
assert.throws(function() { execFile(cmd, a, n, s); }, TypeError);
assert.throws(function() { execFile(cmd, u, o, s); }, TypeError);
assert.throws(function() { execFile(cmd, n, o, s); }, TypeError);
assert.doesNotThrow(function() { execFile(cmd, c, s); });


// verify that fork has same argument parsing behaviour as spawn
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

assert.throws(function() { fork(empty, s); }, TypeError);
assert.throws(function() { fork(empty, a, s); }, TypeError);
