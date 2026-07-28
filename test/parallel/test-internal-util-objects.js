// Flags: --expose-internals
'use strict';
require('../common');

// Test helper objects from internal/util

const assert = require('assert');
const {
  kEnumerableProperty,
  kEmptyObject,
} = require('internal/util');

Object.prototype.blep = 'blop';

{
  assert.strictEqual(
    kEnumerableProperty.blep,
    undefined
  );
  assert.strictEqual(
    kEnumerableProperty.enumerable,
    true
  );
  assert.strictEqual(
    Object.getPrototypeOf(kEnumerableProperty),
    null
  );
  assert.deepStrictEqual(
    Object.getOwnPropertyNames(kEnumerableProperty),
    [ 'enumerable' ]
  );

  assert.throws(
    () => Object.setPrototypeOf(kEnumerableProperty, { value: undefined }),
    TypeError
  );
  assert.throws(
    () => delete kEnumerableProperty.enumerable,
    TypeError
  );
  assert.throws(
    () => kEnumerableProperty.enumerable = false,
    TypeError
  );
  assert.throws(
    () => Object.assign(kEnumerableProperty, { enumerable: false }),
    TypeError
  );
  assert.throws(
    () => kEnumerableProperty.value = undefined,
    TypeError
  );
  assert.throws(
    () => Object.assign(kEnumerableProperty, { value: undefined }),
    TypeError
  );
  assert.throws(
    () => Object.defineProperty(kEnumerableProperty, 'value', {}),
    TypeError
  );
}

{
  assert.strictEqual(
    kEmptyObject.blep,
    undefined
  );
  assert.strictEqual(
    kEmptyObject.prototype,
    undefined
  );
  assert.strictEqual(
    Object.getPrototypeOf(kEmptyObject),
    null
  );
  assert.strictEqual(
    kEmptyObject instanceof Object,
    false
  );
  assert.deepStrictEqual(
    Object.getOwnPropertyDescriptors(kEmptyObject),
    {}
  );
  assert.deepStrictEqual(
    Object.getOwnPropertyNames(kEmptyObject),
    []
  );
  assert.deepStrictEqual(
    Object.getOwnPropertySymbols(kEmptyObject),
    []
  );
  assert.strictEqual(
    Object.isExtensible(kEmptyObject),
    false
  );
  assert.strictEqual(
    Object.isSealed(kEmptyObject),
    true
  );
  assert.strictEqual(
    Object.isFrozen(kEmptyObject),
    true
  );

  assert.throws(
    () => kEmptyObject.foo = 'bar',
    TypeError
  );
  assert.throws(
    () => Object.assign(kEmptyObject, { foo: 'bar' }),
    TypeError
  );
  assert.throws(
    () => Object.defineProperty(kEmptyObject, 'foo', {}),
    TypeError
  );
}

delete Object.prototype.blep;
