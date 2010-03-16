require("../common");

debug('evalcx a string');
var result = process.evalcx('"passed";');
assert.equal('passed', result);

debug('evalcx a thrown error');
assert.throws(function() {
  process.evalcx('throw new Error("test");');
});

hello = 5;
process.evalcx('hello = 2');
assert.equal(5, hello);


code = "foo = 1;"
     + "bar = 2;"
     + "if (baz !== 3) throw new Error('test fail');"
     + "quux.pwned = true;";

foo = 2;
var quux = { pwned : false };
obj = { foo : 0, baz : 3, quux : quux };
var baz = process.evalcx(code, obj);
assert.equal(1, obj.foo);
assert.equal(2, obj.bar);
assert.equal(obj.quux.pwned, true);
assert.equal(quux.pwned, false);
assert.notEqual(quux, obj.quux);

assert.equal(2, foo);
