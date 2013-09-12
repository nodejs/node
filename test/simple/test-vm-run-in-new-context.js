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

// Flags: --expose-gc

var common = require('../common');
var assert = require('assert');
var vm = require('vm');

assert.equal(typeof gc, 'function', 'Run this test with --expose-gc');

common.globalCheck = false;

console.error('run a string');
var result = vm.runInNewContext('\'passed\';');
assert.equal('passed', result);

console.error('thrown error');
assert.throws(function() {
  vm.runInNewContext('throw new Error(\'test\');');
});

hello = 5;
vm.runInNewContext('hello = 2');
assert.equal(5, hello);


console.error('pass values in and out');
code = 'foo = 1;' +
       'bar = 2;' +
       'if (baz !== 3) throw new Error(\'test fail\');';
foo = 2;
obj = { foo: 0, baz: 3 };
var baz = vm.runInNewContext(code, obj);
assert.equal(1, obj.foo);
assert.equal(2, obj.bar);
assert.equal(2, foo);

console.error('call a function by reference');
function changeFoo() { foo = 100 }
vm.runInNewContext('f()', { f: changeFoo });
assert.equal(foo, 100);

console.error('modify an object by reference');
var f = { a: 1 };
vm.runInNewContext('f.a = 2', { f: f });
assert.equal(f.a, 2);

console.error('use function in context without referencing context');
var fn = vm.runInNewContext('(function() { obj.p = {}; })', { obj: {} })
gc();
fn();
// Should not crash
