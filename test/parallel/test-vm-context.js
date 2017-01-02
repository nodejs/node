'use strict';
require('../common');
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
assert.throws(function() {
  vm.runInNewContext('', null, 'some.js');
}, TypeError);

// Issue GH-1140:
console.error('test runInContext signature');
var gh1140Exception;
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
[undefined, null, 0, 0.0, '', {}, []].forEach(function(e) {
  assert.throws(function() { script.runInContext(e); }, TypeError);
  assert.throws(function() { vm.runInContext('', e); }, TypeError);
});

// Issue GH-693:
console.error('test RegExp as argument to assert.throws');
script = vm.createScript('var assert = require(\'assert\'); assert.throws(' +
                         'function() { throw "hello world"; }, /hello/);',
                         'some.js');
script.runInNewContext({ require: require });

// Issue GH-7529
script = vm.createScript('delete b');
var ctx = {};
Object.defineProperty(ctx, 'b', { configurable: false });
ctx = vm.createContext(ctx);
assert.equal(script.runInContext(ctx), false);

// Error on the first line of a module should
// have the correct line and column number
assert.throws(function() {
  vm.runInContext('throw new Error()', context, {
    filename: 'expected-filename.js',
    lineOffset: 32,
    columnOffset: 123
  });
}, function(err) {
  return /expected-filename.js:33:130/.test(err.stack);
}, 'Expected appearance of proper offset in Error stack');

// https://github.com/nodejs/node/issues/6158
ctx = new Proxy({}, {});
assert.strictEqual(typeof vm.runInNewContext('String', ctx), 'function');

// https://github.com/nodejs/node/issues/10223
ctx = vm.createContext();
vm.runInContext('Object.defineProperty(this, "x", { value: 42 })', ctx);
assert.strictEqual(ctx.x, undefined);  // Not copied out by cloneProperty().
assert.strictEqual(vm.runInContext('x', ctx), 42);
vm.runInContext('x = 0', ctx);                      // Does not throw but x...
assert.strictEqual(vm.runInContext('x', ctx), 42);  // ...should be unaltered.
assert.throws(() => vm.runInContext('"use strict"; x = 0', ctx),
              /Cannot assign to read only property 'x'/);
assert.strictEqual(vm.runInContext('x', ctx), 42);
