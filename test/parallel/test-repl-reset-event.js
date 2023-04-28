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
const ArrayStream = require('../common/arraystream');
const assert = require('assert');
const repl = require('repl');
const util = require('util');

common.allowGlobals(42);

// Create a dummy stream that does nothing
const dummy = new ArrayStream();

function testReset(cb) {
  const r = repl.start({
    input: dummy,
    output: dummy,
    useGlobal: false
  });
  r.context.foo = 42;
  r.on('reset', common.mustCall(function(context) {
    assert(!!context, 'REPL did not emit a context with reset event');
    assert.strictEqual(context, r.context, 'REPL emitted incorrect context. ' +
    `context is ${util.inspect(context)}, expected ${util.inspect(r.context)}`);
    assert.strictEqual(
      context.foo,
      undefined,
      'REPL emitted the previous context and is not using global as context. ' +
      `context.foo is ${context.foo}, expected undefined.`
    );
    context.foo = 42;
    cb();
  }));
  r.resetContext();
}

function testResetGlobal() {
  const r = repl.start({
    input: dummy,
    output: dummy,
    useGlobal: true
  });
  r.context.foo = 42;
  r.on('reset', common.mustCall(function(context) {
    assert.strictEqual(
      context.foo,
      42,
      '"foo" property is different from REPL using global as context. ' +
      `context.foo is ${context.foo}, expected 42.`
    );
  }));
  r.resetContext();
}

testReset(common.mustCall(testResetGlobal));
