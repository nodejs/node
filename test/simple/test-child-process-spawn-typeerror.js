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

var assert = require('assert'),
  child_process = require('child_process'),
  spawn = child_process.spawn,
  fork = child_process.fork,
  execFile = child_process.execFile,
  windows = (process.platform === 'win32'),
  cmd = (windows) ? 'rundll32' : 'ls',
  invalidcmd = 'hopefully_you_dont_have_this_on_your_machine',
  invalidArgsMsg = /Incorrect value of args option/,
  invalidOptionsMsg = /options argument must be an object/,
  empty = require('../common').fixturesDir + '/empty.js',
  errors = 0;

try {
  // Ensure this throws a TypeError
  var child = spawn(invalidcmd, 'this is not an array');

  child.on('error', function (err) {
    errors++;
  });

} catch (e) {
  assert.equal(e instanceof TypeError, true);
}

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
}, /Bad argument/);

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

process.on('exit', function() {
  assert.equal(errors, 0);
});

// Argument types for combinatorics
var a=[], o={}, c=(function callback(){}), s='string', u=undefined, n=null;

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

// string is invalid in arg position (this may seem strange, but is
// consistent across node API, cf. `net.createServer('not options', 'not
// callback')`
assert.throws(function() { execFile(cmd, s, o, c); }, TypeError);
assert.doesNotThrow(function() { execFile(cmd, a, s, c); });
assert.doesNotThrow(function() { execFile(cmd, a, o, s); });


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

assert.throws(function() { fork(empty, s); }, TypeError);
assert.doesNotThrow(function() { fork(empty, a, s); }, TypeError);
