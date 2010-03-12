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


code = "foo = 1; bar = 2;";
foo = 2;
obj = { foo : 0 };
process.evalcx(code, obj);

/* TODO?
assert.equal(1, obj.foo);
assert.equal(2, obj.bar);
*/

assert.equal(2, foo);
