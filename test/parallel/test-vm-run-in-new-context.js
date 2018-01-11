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
// Flags: --expose-gc

const common = require('../common');
const assert = require('assert');
const vm = require('vm');

assert.strictEqual(typeof global.gc, 'function',
                   'Run this test with --expose-gc');

common.globalCheck = false;

// Run a string
const result = vm.runInNewContext('\'passed\';');
assert.strictEqual(result, 'passed');

// Thrown error
assert.throws(() => {
  vm.runInNewContext('throw new Error(\'test\');');
}, /^Error: test$/);

global.hello = 5;
vm.runInNewContext('hello = 2');
assert.strictEqual(global.hello, 5);


// Pass values in and out
global.code = 'foo = 1;' +
              'bar = 2;' +
              'if (baz !== 3) throw new Error(\'test fail\');';
global.foo = 2;
global.obj = { foo: 0, baz: 3 };
/* eslint-disable no-unused-vars */
const baz = vm.runInNewContext(global.code, global.obj);
/* eslint-enable no-unused-vars */
assert.strictEqual(global.obj.foo, 1);
assert.strictEqual(global.obj.bar, 2);
assert.strictEqual(global.foo, 2);

// Call a function by reference
function changeFoo() { global.foo = 100; }
vm.runInNewContext('f()', { f: changeFoo });
assert.strictEqual(global.foo, 100);

// Modify an object by reference
const f = { a: 1 };
vm.runInNewContext('f.a = 2', { f });
assert.strictEqual(f.a, 2);

// Use function in context without referencing context
const fn = vm.runInNewContext('(function() { obj.p = {}; })', { obj: {} });
global.gc();
fn();
// Should not crash

{
  // Verify that providing a custom filename as a string argument works.
  const code = 'throw new Error("foo");';
  const file = 'test_file.vm';

  assert.throws(() => {
    vm.runInNewContext(code, {}, file);
  }, (err) => {
    const lines = err.stack.split('\n');

    assert.strictEqual(lines[0].trim(), `${file}:1`);
    assert.strictEqual(lines[1].trim(), code);
    // Skip lines[2] and lines[3]. They're just a ^ and blank line.
    assert.strictEqual(lines[4].trim(), 'Error: foo');
    assert.strictEqual(lines[5].trim(), `at ${file}:1:7`);
    // The rest of the stack is uninteresting.
    return true;
  });
}
