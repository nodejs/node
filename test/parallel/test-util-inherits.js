'use strict';

const common = require('../common');
const assert = require('assert');
const inherits = require('util').inherits;
const errCheck = common.expectsError({
  code: 'ERR_INVALID_ARG_TYPE',
  type: TypeError,
  message: 'The "superCtor" argument must be of type function'
});

// super constructor
function A() {
  this._a = 'a';
}
A.prototype.a = function() { return this._a; };

// one level of inheritance
function B(value) {
  A.call(this);
  this._b = value;
}
inherits(B, A);
B.prototype.b = function() { return this._b; };

assert.strictEqual(B.super_, A);

const b = new B('b');
assert.strictEqual(b.a(), 'a');
assert.strictEqual(b.b(), 'b');
assert.strictEqual(b.constructor, B);

// two levels of inheritance
function C() {
  B.call(this, 'b');
  this._c = 'c';
}
inherits(C, B);
C.prototype.c = function() { return this._c; };
C.prototype.getValue = function() { return this.a() + this.b() + this.c(); };

assert.strictEqual(C.super_, B);

const c = new C();
assert.strictEqual(c.getValue(), 'abc');
assert.strictEqual(c.constructor, C);

// inherits can be called after setting prototype properties
function D() {
  C.call(this);
  this._d = 'd';
}

D.prototype.d = function() { return this._d; };
inherits(D, C);

assert.strictEqual(D.super_, C);

const d = new D();
assert.strictEqual(d.c(), 'c');
assert.strictEqual(d.d(), 'd');
assert.strictEqual(d.constructor, D);

// ES6 classes can inherit from a constructor function
class E {
  constructor() {
    D.call(this);
    this._e = 'e';
  }
  e() { return this._e; }
}
inherits(E, D);

assert.strictEqual(E.super_, D);

const e = new E();
assert.strictEqual(e.getValue(), 'abc');
assert.strictEqual(e.d(), 'd');
assert.strictEqual(e.e(), 'e');
assert.strictEqual(e.constructor, E);

// should throw with invalid arguments
assert.throws(function() {
  inherits(A, {});
}, common.expectsError({
  code: 'ERR_INVALID_ARG_TYPE',
  type: TypeError,
  message: 'The "superCtor.prototype" property must be of type function'
})
);
assert.throws(function() {
  inherits(A, null);
}, errCheck);
assert.throws(function() {
  inherits(null, A);
}, common.expectsError({
  code: 'ERR_INVALID_ARG_TYPE',
  type: TypeError,
  message: 'The "ctor" argument must be of type function'
})
);
