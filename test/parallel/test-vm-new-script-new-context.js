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
const Script = require('vm').Script;

common.globalCheck = false;

console.error('run a string');
let script = new Script('\'passed\';');
console.error('script created');
const result1 = script.runInNewContext();
const result2 = script.runInNewContext();
assert.strictEqual('passed', result1);
assert.strictEqual('passed', result2);

console.error('thrown error');
script = new Script('throw new Error(\'test\');');
assert.throws(function() {
  script.runInNewContext();
}, /test/);


console.error('undefined reference');
script = new Script('foo.bar = 5;');
assert.throws(function() {
  script.runInNewContext();
}, /not defined/);


global.hello = 5;
script = new Script('hello = 2');
script.runInNewContext();
assert.strictEqual(5, global.hello);


console.error('pass values in and out');
global.code = 'foo = 1;' +
              'bar = 2;' +
              'if (baz !== 3) throw new Error(\'test fail\');';
global.foo = 2;
global.obj = { foo: 0, baz: 3 };
script = new Script(global.code);
/* eslint-disable no-unused-vars */
const baz = script.runInNewContext(global.obj);
/* eslint-enable no-unused-vars */
assert.strictEqual(1, global.obj.foo);
assert.strictEqual(2, global.obj.bar);
assert.strictEqual(2, global.foo);

console.error('call a function by reference');
script = new Script('f()');
function changeFoo() { global.foo = 100; }
script.runInNewContext({ f: changeFoo });
assert.strictEqual(global.foo, 100);

console.error('modify an object by reference');
script = new Script('f.a = 2');
const f = { a: 1 };
script.runInNewContext({ f: f });
assert.strictEqual(f.a, 2);

assert.throws(function() {
  script.runInNewContext();
}, /f is not defined/);

console.error('invalid this');
assert.throws(function() {
  script.runInNewContext.call('\'hello\';');
}, TypeError);
