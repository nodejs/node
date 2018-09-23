// Flags: --expose-internals
'use strict';

require('../common');
const assert = require('assert');
const util = require('internal/util');

const createClassWrapper = util.createClassWrapper;

class A {
  constructor(a, b, c) {
    this.a = a;
    this.b = b;
    this.c = c;
  }
}

const B = createClassWrapper(A);

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
