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
const Stream = require('stream');
const Console = require('console').Console;

const out = new Stream();
const err = new Stream();

// ensure the Console instance doesn't write to the
// process' "stdout" or "stderr" streams
process.stdout.write = process.stderr.write = common.mustNotCall();

// make sure that the "Console" function exists
assert.strictEqual('function', typeof Console);

// make sure that the Console constructor throws
// when not given a writable stream instance
common.expectsError(
  () => { new Console(); },
  {
    code: 'ERR_CONSOLE_WRITABLE_STREAM',
    type: TypeError,
    message: /stdout/
  }
);

// Console constructor should throw if stderr exists but is not writable
common.expectsError(
  () => {
    out.write = () => {};
    err.write = undefined;
    new Console(out, err);
  },
  {
    code: 'ERR_CONSOLE_WRITABLE_STREAM',
    type: TypeError,
    message: /stderr/
  }
);

out.write = err.write = (d) => {};

const c = new Console(out, err);

out.write = err.write = common.mustCall((d) => {
  assert.strictEqual(d, 'test\n');
}, 2);

c.log('test');
c.error('test');

out.write = common.mustCall((d) => {
  assert.strictEqual('{ foo: 1 }\n', d);
});

c.dir({ foo: 1 });

// ensure that the console functions are bound to the console instance
let called = 0;
out.write = common.mustCall((d) => {
  called++;
  assert.strictEqual(d, `${called} ${called - 1} [ 1, 2, 3 ]\n`);
}, 3);

[1, 2, 3].forEach(c.log);

// Console() detects if it is called without `new` keyword
assert.doesNotThrow(() => {
  Console(out, err);
});

// Instance that does not ignore the stream errors.
const c2 = new Console(out, err, false);

out.write = () => { throw new Error('out'); };
err.write = () => { throw new Error('err'); };

assert.throws(() => c2.log('foo'), /^Error: out$/);
assert.throws(() => c2.warn('foo'), /^Error: err$/);
assert.throws(() => c2.dir('foo'), /^Error: out$/);
