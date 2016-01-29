var ArrayIndex = require('./')
var inherits = require('util').inherits
var assert = require('assert')


/**
 * Create a "subclass".
 */

function Arrayish (length) {
  ArrayIndex.call(this, length)
  this.sets = Object.create(null)
}

// inherit from `ArrayIndex`
inherits(Arrayish, ArrayIndex)


// create an instance and run some tests
var a = new Arrayish(11)
assert.equal(a.length, 11);

assert.throws(function () {
  a[0]
}, /you must implement the `ArrayIndex.get` Symbol/)

assert.throws(function () {
  a[0] = 0
}, /you must implement the `ArrayIndex.set` Symbol/)


/**
 * This "getter" function checks if the index has previosly been "set", and if so
 * returns the index * the value previously set. If the index hasn't been set,
 * return the index as-is.
 */

Arrayish.prototype[ArrayIndex.get] = function get (index) {
  if (index in this.sets) {
    return +this.sets[index] * index
  } else {
    return index
  }
}

/**
 * Store the last value set for this index.
 */

Arrayish.prototype[ArrayIndex.set] = function set (index, value) {
  this.sets[index] = value
}


// test getters without being "set"
assert.equal(0, a[0])
assert.equal(1, a[1])
assert.equal(2, a[2])
assert.equal(3, a[3])
assert.equal(4, a[4])

// test setters, followed by getters
a[10] = 1
assert.equal(10, a[10])
a[10] = 2
assert.equal(20, a[10])
a[10] = 3
assert.equal(30, a[10])

// test "length"
assert.equal(11, a.length)

a[4] = 20
a[6] = 5.55432
var b = [0, 1, 2, 3, 80, 5, 33.325919999999996, 7, 8, 9, 30]
assert.equal(JSON.stringify(b), JSON.stringify(a))


/**
 * It should work when invoking as a Mixin.
 */

function Foo () {
  ArrayIndex.call(this, 5);
}
var f = new Foo();

// these throw because there's no __get__ and __set__ function defined
assert.throws(function () {
  f[0];
});
assert.throws(function () {
  f[0] = 0
});

f[ArrayIndex.get] = function (index) {
  return index * 2;
};

assert.equal(f[0], 0);
assert.equal(f[1], 2);
assert.equal(f[2], 4);
assert.equal(f[3], 6);

f[ArrayIndex.set] = function (index, value) {
  this['foo' + index] = value;
};

f[1] = 'bar';
assert.equal(f.foo1, 'bar');
