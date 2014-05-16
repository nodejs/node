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

var vm = require('vm');
var Script = vm.Script;
var script = new Script('"passed";');

console.error('run in a new empty context');
var context = vm.createContext();
var result = script.runInContext(context);
assert.equal('passed', result);

console.error('create a new pre-populated context');
context = vm.createContext({'foo': 'bar', 'thing': 'lala'});
assert.equal('bar', context.foo);
assert.equal('lala', context.thing);

console.error('test updating context');
script = new Script('foo = 3;');
result = script.runInContext(context);
assert.equal(3, context.foo);
assert.equal('lala', context.thing);

// Issue GH-227:
assert.throws(function () {
  vm.runInNewContext('', null, 'some.js');
}, TypeError);

// Issue GH-1140:
console.error('test runInContext signature');
var gh1140Exception;
try {
  vm.runInContext('throw new Error()', context, 'expected-filename.js');
}
catch (e) {
  gh1140Exception = e;
  assert.ok(/expected-filename/.test(e.stack),
            'expected appearance of filename in Error stack');
}
assert.ok(gh1140Exception,
          'expected exception from runInContext signature test');

// GH-558, non-context argument segfaults / raises assertion
[undefined, null, 0, 0.0, '', {}, []].forEach(function(e) {
  assert.throws(function() { script.runInContext(e); }, TypeError);
  assert.throws(function() { vm.runInContext('', e); }, TypeError);
});

// Issue GH-693:
console.error('test RegExp as argument to assert.throws');
script = vm.createScript('var assert = require(\'assert\'); assert.throws(' +
                         'function() { throw "hello world"; }, /hello/);',
                         'some.js');
script.runInNewContext({ require : require });

// Issue GH-7529
script = vm.createScript('delete b');
var ctx = {};
Object.defineProperty(ctx, 'b', { configurable: false });
ctx = vm.createContext(ctx);
assert.equal(script.runInContext(ctx), false);
