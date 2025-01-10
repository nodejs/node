'use strict';

require('../common');

const {
  describe,
  it,
} = require('node:test');

const {
  strictEqual,
} = require('node:assert');

class Foo {
  refCalled = 0;
  unrefCalled = 0;
  ref() {
    this.refCalled++;
  }
  unref() {
    this.unrefCalled++;
  }
}

class Foo2 {
  refCalled = 0;
  unrefCalled = 0;
  [Symbol.for('nodejs.ref')]() {
    this.refCalled++;
  }
  [Symbol.for('nodejs.unref')]() {
    this.unrefCalled++;
  }
}

// TODO(aduh95): remove support for undocumented symbol
class Foo3 {
  refCalled = 0;
  unrefCalled = 0;
  [Symbol.for('node:ref')]() {
    this.refCalled++;
  }
  [Symbol.for('node:unref')]() {
    this.unrefCalled++;
  }
}

describe('process.ref/unref work as expected', () => {
  it('refs...', () => {
    // Objects that implement the new Symbol-based API
    // just work.
    const foo1 = new Foo();
    const foo2 = new Foo2();
    const foo3 = new Foo3();
    process.ref(foo1);
    process.unref(foo1);
    process.ref(foo2);
    process.unref(foo2);
    process.ref(foo3);
    process.unref(foo3);
    strictEqual(foo1.refCalled, 1);
    strictEqual(foo1.unrefCalled, 1);
    strictEqual(foo2.refCalled, 1);
    strictEqual(foo2.unrefCalled, 1);
    strictEqual(foo3.refCalled, 1);
    strictEqual(foo3.unrefCalled, 1);

    // Objects that implement the legacy API also just work.
    const i = setInterval(() => {}, 1000);
    strictEqual(i.hasRef(), true);
    process.unref(i);
    strictEqual(i.hasRef(), false);
    process.ref(i);
    strictEqual(i.hasRef(), true);
    clearInterval(i);
  });
});
