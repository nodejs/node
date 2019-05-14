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

const vm = require('vm');
const Script = vm.Script;
let script = new Script('"passed";');

// Run in a new empty context
let context = vm.createContext();
let result = script.runInContext(context);
assert.strictEqual(result, 'passed');

// Create a new pre-populated context
context = vm.createContext({ 'foo': 'bar', 'thing': 'lala' });
assert.strictEqual(context.foo, 'bar');
assert.strictEqual(context.thing, 'lala');

// Test updating context
script = new Script('foo = 3;');
result = script.runInContext(context);
assert.strictEqual(context.foo, 3);
assert.strictEqual(context.thing, 'lala');

// Issue GH-227:
common.expectsError(() => {
  vm.runInNewContext('', null, 'some.js');
}, {
  code: 'ERR_INVALID_ARG_TYPE',
  type: TypeError
});

// Issue GH-1140:
// Test runInContext signature
let gh1140Exception;
try {
  vm.runInContext('throw new Error()', context, 'expected-filename.js');
} catch (e) {
  gh1140Exception = e;
  assert.ok(/expected-filename/.test(e.stack),
            `expected appearance of filename in Error stack: ${e.stack}`);
}
// This is outside of catch block to confirm catch block ran.
assert.strictEqual(gh1140Exception.toString(), 'Error');

const nonContextualSandboxError = {
  code: 'ERR_INVALID_ARG_TYPE',
  type: TypeError,
  message: /must be of type Object/
};
const contextifiedSandboxError = {
  code: 'ERR_INVALID_ARG_TYPE',
  type: TypeError,
  message: /must be of type vm\.Context/
};

[
  [undefined, nonContextualSandboxError],
  [null, nonContextualSandboxError], [0, nonContextualSandboxError],
  [0.0, nonContextualSandboxError], ['', nonContextualSandboxError],
  [{}, contextifiedSandboxError], [[], contextifiedSandboxError]
].forEach((e) => {
  common.expectsError(() => { script.runInContext(e[0]); }, e[1]);
  common.expectsError(() => { vm.runInContext('', e[0]); }, e[1]);
});

// Issue GH-693:
// Test RegExp as argument to assert.throws
script = vm.createScript('const assert = require(\'assert\'); assert.throws(' +
                         'function() { throw "hello world"; }, /hello/);',
                         'some.js');
script.runInNewContext({ require });

// Issue GH-7529
script = vm.createScript('delete b');
let ctx = {};
Object.defineProperty(ctx, 'b', { configurable: false });
ctx = vm.createContext(ctx);
assert.strictEqual(script.runInContext(ctx), false);

// Error on the first line of a module should have the correct line and column
// number.
{
  let stack = null;
  assert.throws(() => {
    vm.runInContext(' throw new Error()', context, {
      filename: 'expected-filename.js',
      lineOffset: 32,
      columnOffset: 123
    });
  }, (err) => {
    stack = err.stack;
    return /^ \^/m.test(stack) &&
           /expected-filename\.js:33:131/.test(stack);
  }, `stack not formatted as expected: ${stack}`);
}

// https://github.com/nodejs/node/issues/6158
ctx = new Proxy({}, {});
assert.strictEqual(typeof vm.runInNewContext('String', ctx), 'function');
