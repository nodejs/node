var assert = require("assert");
var makeAccessor = require("../private").makeAccessor;
var acc1 = makeAccessor();
var obj = {};
var hasOwn = obj.hasOwnProperty;

acc1(obj).foo = 42;
assert.deepEqual(obj, {});
assert.deepEqual(acc1(obj), { foo: 42 });
assert.deepEqual(acc1(acc1(obj)), {});
assert.deepEqual(acc1(obj), { foo: 42 });
assert.deepEqual(Object.keys(acc1(obj)), ["foo"]);
assert.strictEqual(Object.getOwnPropertyNames(acc1(obj)).length, 1);
assert.strictEqual(Object.getOwnPropertyNames(acc1(acc1(obj))).length, 0);
acc1(obj).bar = "baz";
assert.deepEqual(acc1(obj), { foo: 42, bar: "baz" });
delete acc1(obj).foo;
assert.deepEqual(acc1(obj), { bar: "baz" });

try {
  acc1(42);
  throw new Error("threw wrong error");
} catch (err) {
  assert.ok(err);
}

var acc2 = makeAccessor();
assert.notStrictEqual(acc1, acc2);
assert.notStrictEqual(acc1(obj), acc2(obj));
assert.deepEqual(acc2(obj), {});
assert.strictEqual(Object.getOwnPropertyNames(obj).length, 0);
assert.strictEqual(Object.keys(obj).length, 0);
acc2(obj).bar = "asdf";
assert.deepEqual(acc2(obj), { bar: "asdf" });

acc2.forget(obj);
acc2(obj).bar = "asdf";
var oldSecret = acc2(obj);
assert.strictEqual(oldSecret.bar, "asdf");
acc2.forget(obj);
var newSecret = acc2(obj);
assert.notStrictEqual(oldSecret, newSecret);
assert.ok(hasOwn.call(oldSecret, "bar"));
assert.ok(!hasOwn.call(newSecret, "bar"));
newSecret.bar = "zxcv";
assert.strictEqual(oldSecret.bar, "asdf");
assert.strictEqual(acc2(obj).bar, "zxcv");

function creatorFn(object) {
  return { self: object };
}

var acc3 = makeAccessor(creatorFn);

acc3(obj).xxx = "yyy";
assert.deepEqual(acc3(obj), {
  self: obj,
  xxx: "yyy"
});

acc3.forget(obj);
assert.deepEqual(acc3(obj), {
  self: obj
});

var green = "\033[32m";
var reset = "\033[0m";
console.log(green + "ALL PASS" + reset);
