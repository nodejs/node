'use strict';

require('../common');

// This test ensures that util.inspect logs getters
// which access this.

const assert = require('assert');

const { inspect } = require('util');

{
  class X {
    constructor() {
      this._y = 123;
    }

    get y() {
      return this._y;
    }
  }

  const result = inspect(new X(), {
    getters: true,
    showHidden: true
  });

  assert.strictEqual(
    result,
    'X { _y: 123, [y]: [Getter: 123] }'
  );
}

// Regression test for https://github.com/nodejs/node/issues/37054
{
  class A {
    constructor(B) {
      this.B = B;
    }
    get b() {
      return this.B;
    }
  }

  class B {
    constructor() {
      this.A = new A(this);
    }
    get a() {
      return this.A;
    }
  }

  const result = inspect(new B(), {
    depth: 1,
    getters: true,
    showHidden: true
  });

  assert.strictEqual(
    result,
    '<ref *1> B {\n' +
    '  A: A { B: [Circular *1], [b]: [Getter] [Circular *1] },\n' +
    '  [a]: [Getter] A { B: [Circular *1], [b]: [Getter] [Circular *1] }\n' +
    '}',
  );
}
