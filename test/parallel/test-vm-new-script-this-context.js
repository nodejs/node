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

var common = require('../common');
var assert = require('assert');
var Script = require('vm').Script;

common.globalCheck = false;

console.error('run a string');
var script = new Script('\'passed\';');
var result = script.runInThisContext(script);
assert.equal('passed', result);

console.error('thrown error');
script = new Script('throw new Error(\'test\');');
assert.throws(function() {
  script.runInThisContext(script);
});

hello = 5;
script = new Script('hello = 2');
script.runInThisContext(script);
assert.equal(2, hello);


console.error('pass values');
code = 'foo = 1;' +
       'bar = 2;' +
       'if (typeof baz !== \'undefined\') throw new Error(\'test fail\');';
foo = 2;
obj = { foo: 0, baz: 3 };
script = new Script(code);
script.runInThisContext(script);
assert.equal(0, obj.foo);
assert.equal(2, bar);
assert.equal(1, foo);

console.error('call a function');
f = function() { foo = 100 };
script = new Script('f()');
script.runInThisContext(script);
assert.equal(100, foo);
