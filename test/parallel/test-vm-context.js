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
require('../common');
const assert = require('assert');

const vm = require('vm');
const Script = vm.Script;
let script = new Script('"passed";');

// Run in a new empty context
let context = vm.createContext();
let result = script.runInContext(context);
assert.strictEqual('passed', result);

// Create a new pre-populated context
context = vm.createContext({ 'foo': 'bar', 'thing': 'lala' });
assert.strictEqual('bar', context.foo);
assert.strictEqual('lala', context.thing);

// Test updating context
script = new Script('foo = 3;');
result = script.runInContext(context);
assert.strictEqual(3, context.foo);
assert.strictEqual('lala', context.thing);

// Issue GH-227:
assert.throws(() => {
  vm.runInNewContext('', null, 'some.js');
}, /^TypeError: sandbox must be an object$/);

// Issue GH-1140:
// Test runInContext signature
let gh1140Exception;
try {
  vm.runInContext('throw new Error()', context, 'expected-filename.js');
} catch (e) {
  gh1140Exception = e;
  assert.ok(/expected-filename/.test(e.stack),
            'expected appearance of filename in Error stack');
}
assert.ok(gh1140Exception,
          'expected exception from runInContext signature test');

// GH-558, non-context argument segfaults / raises assertion
const nonContextualSandboxErrorMsg =
  /^TypeError: contextifiedSandbox argument must be an object\.$/;
const contextifiedSandboxErrorMsg =
    /^TypeError: sandbox argument must have been converted to a context\.$/;
[
  [undefined, nonContextualSandboxErrorMsg],
  [null, nonContextualSandboxErrorMsg], [0, nonContextualSandboxErrorMsg],
  [0.0, nonContextualSandboxErrorMsg], ['', nonContextualSandboxErrorMsg],
  [{}, contextifiedSandboxErrorMsg], [[], contextifiedSandboxErrorMsg]
].forEach((e) => {
  assert.throws(() => { script.runInContext(e[0]); }, e[1]);
  assert.throws(() => { vm.runInContext('', e[0]); }, e[1]);
});

// Issue GH-693:
// Test RegExp as argument to assert.throws
script = vm.createScript('const assert = require(\'assert\'); assert.throws(' +
                         'function() { throw "hello world"; }, /hello/);',
                         'some.js');
script.runInNewContext({ require: require });

// Issue GH-7529
script = vm.createScript('delete b');
let ctx = {};
Object.defineProperty(ctx, 'b', { configurable: false });
ctx = vm.createContext(ctx);
assert.strictEqual(script.runInContext(ctx), false);

// Error on the first line of a module should
// have the correct line and column number
assert.throws(() => {
  vm.runInContext('throw new Error()', context, {
    filename: 'expected-filename.js',
    lineOffset: 32,
    columnOffset: 123
  });
}, (err) => {
  return /expected-filename\.js:33:130/.test(err.stack);
}, 'Expected appearance of proper offset in Error stack');

// https://github.com/nodejs/node/issues/6158
ctx = new Proxy({}, {});
assert.strictEqual(typeof vm.runInNewContext('String', ctx), 'function');

// https://github.com/nodejs/node/issues/10223
ctx = vm.createContext();
vm.runInContext('Object.defineProperty(this, "x", { value: 42 })', ctx);
assert.strictEqual(ctx.x, 42);
assert.strictEqual(vm.runInContext('x', ctx), 42);

vm.runInContext('x = 0', ctx);                      // Does not throw but x...
assert.strictEqual(vm.runInContext('x', ctx), 42);  // ...should be unaltered.

assert.throws(() => vm.runInContext('"use strict"; x = 0', ctx),
              /Cannot assign to read only property 'x'/);
assert.strictEqual(vm.runInContext('x', ctx), 42);
