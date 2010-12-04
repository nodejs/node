var common = require('../common');
var assert = require('assert');
var Script = require('vm').Script;

common.globalCheck = false;

common.debug('run a string');
var script = new Script('\'passed\';');
var result = script.runInThisContext(script);
assert.equal('passed', result);

common.debug('thrown error');
script = new Script('throw new Error(\'test\');');
assert.throws(function() {
  script.runInThisContext(script);
});

hello = 5;
script = new Script('hello = 2');
script.runInThisContext(script);
assert.equal(2, hello);


common.debug('pass values');
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

common.debug('call a function');
f = function() { foo = 100 };
script = new Script('f()');
script.runInThisContext(script);
assert.equal(100, foo);
