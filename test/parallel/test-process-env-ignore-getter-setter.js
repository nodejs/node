'use strict';
require('../common');
const assert = require('assert');

assert.throws(
  () => {
    Object.defineProperty(process.env, 'foo', {
      value: 'foo1'
    });
  },
  {
    code: 'ERR_INVALID_OBJECT_DEFINE_PROPERTY',
    name: 'TypeError',
  }
);

assert.strictEqual(process.env.foo, undefined);
process.env.foo = 'foo2';
assert.strictEqual(process.env.foo, 'foo2');

assert.throws(
  () => {
    Object.defineProperty(process.env, 'goo', {
      get() {
        return 'goo';
      },
      set() {}
    });
  },
  {
    code: 'ERR_INVALID_OBJECT_DEFINE_PROPERTY',
    name: 'TypeError',
  }
);

const attributes = ['configurable', 'writable', 'enumerable'];

attributes.forEach((attribute) => {
  assert.throws(
    () => {
      Object.defineProperty(process.env, 'goo', {
        [attribute]: false
      });
    },
    {
      code: 'ERR_INVALID_OBJECT_DEFINE_PROPERTY',
      name: 'TypeError',
    }
  );
});

assert.strictEqual(process.env.goo, undefined);
Object.defineProperty(process.env, 'goo', {
  value: 'goo',
  configurable: true,
  writable: true,
  enumerable: true
});
assert.strictEqual(process.env.goo, 'goo');
