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

// Run a string
let script = new Script('\'passed\';');
const result = script.runInThisContext(script);
assert.strictEqual('passed', result);

// Thrown error
script = new Script('throw new Error(\'test\');');
assert.throws(() => {
  script.runInThisContext(script);
}, /^Error: test$/);

global.hello = 5;
script = new Script('hello = 2');
script.runInThisContext(script);
assert.strictEqual(2, global.hello);


// Pass values
global.code = 'foo = 1;' +
              'bar = 2;' +
              'if (typeof baz !== "undefined") throw new Error("test fail");';
global.foo = 2;
global.obj = { foo: 0, baz: 3 };
script = new Script(global.code);
script.runInThisContext(script);
assert.strictEqual(0, global.obj.foo);
assert.strictEqual(2, global.bar);
assert.strictEqual(1, global.foo);

// Call a function
global.f = function() { global.foo = 100; };
script = new Script('f()');
script.runInThisContext(script);
assert.strictEqual(100, global.foo);
