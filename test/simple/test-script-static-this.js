require("../common");

var Script = process.binding('evals').Script;

debug('run a string');
var result = Script.runInThisContext('"passed";');
assert.equal('passed', result);

debug('thrown error');
assert.throws(function() {
  Script.runInThisContext('throw new Error("test");');
});

hello = 5;
Script.runInThisContext('hello = 2');
assert.equal(2, hello);


debug("pass values");
code = "foo = 1;"
     + "bar = 2;"
     + "if (typeof baz !== 'undefined') throw new Error('test fail');";
foo = 2;
obj = { foo : 0, baz : 3 };
var baz = Script.runInThisContext(code);
assert.equal(0, obj.foo);
assert.equal(2, bar);
assert.equal(1, foo);

debug("call a function");
f = function () { foo = 100 };
Script.runInThisContext("f()");
assert.equal(100, foo);
