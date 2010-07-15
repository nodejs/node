common = require("../common");
assert = common.assert

common.debug('evalcx a string');
var result = process.evalcx('"passed";');
assert.equal('passed', result);

common.debug('evalcx a thrown error');
assert.throws(function() {
  process.evalcx('throw new Error("test");');
});

hello = 5;
process.evalcx('hello = 2');
assert.equal(5, hello);


common.debug("pass values in and out");
code = "foo = 1;"
     + "bar = 2;"
     + "if (baz !== 3) throw new Error('test fail');";
foo = 2;
obj = { foo : 0, baz : 3 };
var baz = process.evalcx(code, obj);
assert.equal(1, obj.foo);
assert.equal(2, obj.bar);
assert.equal(2, foo);

common.debug("call a function by reference");
function changeFoo () { foo = 100 }
process.evalcx("f()", { f : changeFoo });
assert.equal(foo, 100);

common.debug("modify an object by reference");
var f = { a : 1 };
process.evalcx("f.a = 2", { f : f });
assert.equal(f.a, 2);

