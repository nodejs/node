'use strict';
const common = require('../common');
const assert = require('assert');
const Script = require('vm').Script;

common.globalCheck = false;

console.error('run a string');
var script = new Script('\'passed\';');
console.error('script created');
const result1 = script.runInNewContext();
const result2 = script.runInNewContext();
assert.equal('passed', result1);
assert.equal('passed', result2);

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
assert.equal(5, global.hello);


console.error('pass values in and out');
global.code = 'foo = 1;' +
              'bar = 2;' +
              'if (baz !== 3) throw new Error(\'test fail\');';
global.foo = 2;
global.obj = { foo: 0, baz: 3 };
script = new Script(global.code);
/* eslint-disable no-unused-vars */
var baz = script.runInNewContext(global.obj);
/* eslint-enable no-unused-vars */
assert.equal(1, global.obj.foo);
assert.equal(2, global.obj.bar);
assert.equal(2, global.foo);

console.error('call a function by reference');
script = new Script('f()');
function changeFoo() { global.foo = 100; }
script.runInNewContext({ f: changeFoo });
assert.equal(global.foo, 100);

console.error('modify an object by reference');
script = new Script('f.a = 2');
var f = { a: 1 };
script.runInNewContext({ f: f });
assert.equal(f.a, 2);

assert.throws(function() {
  script.runInNewContext();
}, /f is not defined/);

console.error('invalid this');
assert.throws(function() {
  script.runInNewContext.call('\'hello\';');
}, TypeError);
