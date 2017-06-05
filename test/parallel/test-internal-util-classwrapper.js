// Flags: --expose-internals
'use strict';

require('../common');
const assert = require('assert');
const util = require('internal/util');
const { inherits } = require('util');

const createClassWrapper = util.createClassWrapper;

class A {
  constructor() {
    this[util.constructor](...arguments);
  }

  [util.constructor](a, b, c) {
    this.a = a;
    this.b = b;
    this.c = c;
  }
}

const B = createClassWrapper(A);

B.prototype.func = function() {};

assert.strictEqual(typeof B, 'function');
assert(B(1, 2, 3) instanceof B);
assert(B(1, 2, 3) instanceof A);
assert(new B(1, 2, 3) instanceof B);
assert(new B(1, 2, 3) instanceof A);
assert.strictEqual(B.name, A.name);
assert.strictEqual(B.length, A.length);

const b = new B(1, 2, 3);
assert.strictEqual(b.a, 1);
assert.strictEqual(b.b, 2);
assert.strictEqual(b.c, 3);
assert.strictEqual(b.func, B.prototype.func);

function C(a, b, c) {
  if (!(this instanceof C)) {
    return new C(a, b, c);
  }
  B.call(this, a, b, c);
}

inherits(C, B);

C.prototype.kaboom = function() {};

const c = new C(4, 2, 3);
assert.strictEqual(c.a, 4);
assert.strictEqual(c.b, 2);
assert.strictEqual(c.c, 3);
assert.strictEqual(c.kaboom, C.prototype.kaboom);
assert.strictEqual(c.func, B.prototype.func);

const c2 = C(4, 2, 3);
assert.strictEqual(c2.a, 4);
assert.strictEqual(c2.b, 2);
assert.strictEqual(c2.c, 3);
assert.strictEqual(c2.kaboom, C.prototype.kaboom);
assert.strictEqual(c2.func, B.prototype.func);
