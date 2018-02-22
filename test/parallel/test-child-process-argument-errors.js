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

const empty = fixtures.path('empty.js');
const cmd = common.isWindows ? 'rundll32' : 'ls';

// Argument types for combinatorics.
const a = [];
const o = {};
function c() {}
const s = 'string';
const u = undefined;
const n = null;

const invalidSignatureError =
  (fn, message) => common.expectsError(fn, {
    code: 'ERR_INVALID_SIGNATURE',
    type: TypeError,
    message: message
  });

// Verify that valid argument combinations do not throw.
{
  spawn(cmd);
  spawn(cmd, []);
  spawn(cmd, {});
  spawn(cmd, [], {});

  // function spawn(file=f [,args=a] [, options=o]) has valid combinations:
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
  spawn(cmd, a, u);
}

// Verify that invalid argument combinations throw.
{
  const invalidArgTypeError =
    common.expectsError({ code: 'ERR_INVALID_ARG_TYPE', type: TypeError }, 12);

  assert.throws(function() {
    const child = spawn(cmd, 'this is not an array');
    child.on('error', common.mustNotCall());
  }, invalidArgTypeError);

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

  assert.throws(function() { spawn(cmd, n, o); }, invalidArgTypeError);
  assert.throws(function() { spawn(cmd, a, n); }, invalidArgTypeError);

  assert.throws(function() { spawn(cmd, s); }, invalidArgTypeError);
  assert.throws(function() { spawn(cmd, a, s); }, invalidArgTypeError);
}

// Verify that valid argument combinations do not throw.
{
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
  execFile(cmd, c, s);
}

// Verify that invalid argument combinations throw.
{
  const errorStart =
    'Function was invoked with an unknown combination of arguments.\n' +
    'Expected child_process.execFile(file[, args][, options][, callback])\n';

  // String is invalid in arg position (this may seem strange, but is
  // consistent across node API, cf. `net.createServer('not options', 'not
  // callback')`
  invalidSignatureError(
    () => execFile(cmd, s, o, c),
    errorStart +
      `Received child_process.execFile('${cmd}', 'string', {}, [Function: c])`
  );

  invalidSignatureError(
    () => execFile(cmd, a, s, c),
    errorStart +
    `Received child_process.execFile('${cmd}', [], 'string', [Function: c])`
  );

  invalidSignatureError(
    () => execFile(cmd, a, o, s),
    errorStart +
    `Received child_process.execFile('${cmd}', [], {}, 'string')`
  );

  invalidSignatureError(
    () => execFile(cmd, a, s),
    errorStart +
    `Received child_process.execFile('${cmd}', [], 'string')`
  );

  invalidSignatureError(
    () => execFile(cmd, o, s),
    errorStart +
    `Received child_process.execFile('${cmd}', {}, 'string')`
  );

  invalidSignatureError(
    () => execFile(cmd, u, u, s),
    errorStart +
    `Received child_process.execFile('${cmd}', undefined, undefined, 'string')`
  );

  invalidSignatureError(
    () => execFile(cmd, n, n, s),
    errorStart +
    `Received child_process.execFile('${cmd}', null, null, 'string')`
  );

  invalidSignatureError(
    () => execFile(cmd, a, u, s),
    errorStart +
    `Received child_process.execFile('${cmd}', [], undefined, 'string')`
  );

  invalidSignatureError(
    () => execFile(cmd, a, n, s),
    errorStart +
    `Received child_process.execFile('${cmd}', [], null, 'string')`
  );

  invalidSignatureError(
    () => execFile(cmd, u, o, s),
    errorStart +
    `Received child_process.execFile('${cmd}', undefined, {}, 'string')`
  );

  invalidSignatureError(
    () => execFile(cmd, n, o, s),
    errorStart +
    `Received child_process.execFile('${cmd}', null, {}, 'string')`
  );
}

// Verify that invalid argument combinations throw.
{
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
}

// Verify that invalid argument combinations throw.
{
  const errorStart =
    'Function was invoked with an unknown combination of arguments.\n' +
    'Expected child_process.fork(modulePath[, args][, options])\n';

  invalidSignatureError(
    () => fork(empty, s),
    errorStart +
    `Received child_process.fork('${empty}', 'string')`
  );

  invalidSignatureError(
    () => fork(empty, a, s),
    errorStart +
    `Received child_process.fork('${empty}', [], 'string')`
  );
}
