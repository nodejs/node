'use strict';

require('../common');
const assert = require('assert');
const inherits = require('util').inherits;

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

// should throw with invalid arguments
assert.throws(function() { inherits(A, {}); }, TypeError);
assert.throws(function() { inherits(A, null); }, TypeError);
assert.throws(function() { inherits(null, A); }, TypeError);
