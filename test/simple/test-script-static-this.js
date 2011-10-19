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

common.debug('run a string');
var result = Script.runInThisContext('\'passed\';');
assert.equal('passed', result);

common.debug('thrown error');
assert.throws(function() {
  Script.runInThisContext('throw new Error(\'test\');');
});

hello = 5;
Script.runInThisContext('hello = 2');
assert.equal(2, hello);


common.debug('pass values');
code = 'foo = 1;' +
       'bar = 2;' +
       'if (typeof baz !== \'undefined\') throw new Error(\'test fail\');';
foo = 2;
obj = { foo: 0, baz: 3 };
var baz = Script.runInThisContext(code);
assert.equal(0, obj.foo);
assert.equal(2, bar);
assert.equal(1, foo);

common.debug('call a function');
f = function() { foo = 100 };
Script.runInThisContext('f()');
assert.equal(100, foo);
