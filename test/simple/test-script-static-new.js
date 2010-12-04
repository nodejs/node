var common = require('../common');
var assert = require('assert');
var Script = require('vm').Script;

common.globalCheck = false;

common.debug('run a string');
var result = Script.runInNewContext('\'passed\';');
assert.equal('passed', result);

common.debug('thrown error');
assert.throws(function() {
  Script.runInNewContext('throw new Error(\'test\');');
});

hello = 5;
Script.runInNewContext('hello = 2');
assert.equal(5, hello);


common.debug('pass values in and out');
code = 'foo = 1;' +
       'bar = 2;' +
       'if (baz !== 3) throw new Error(\'test fail\');';
foo = 2;
obj = { foo: 0, baz: 3 };
var baz = Script.runInNewContext(code, obj);
assert.equal(1, obj.foo);
assert.equal(2, obj.bar);
assert.equal(2, foo);

common.debug('call a function by reference');
function changeFoo() { foo = 100 }
Script.runInNewContext('f()', { f: changeFoo });
assert.equal(foo, 100);

common.debug('modify an object by reference');
var f = { a: 1 };
Script.runInNewContext('f.a = 2', { f: f });
assert.equal(f.a, 2);

