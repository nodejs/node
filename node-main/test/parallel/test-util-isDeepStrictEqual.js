'use strict';

// Confirm functionality of `util.isDeepStrictEqual()`.

require('../common');

const assert = require('assert');
const util = require('util');
const { test } = require('node:test');

function utilIsDeepStrict(a, b) {
  assert.strictEqual(util.isDeepStrictEqual(a, b), true);
  assert.strictEqual(util.isDeepStrictEqual(b, a), true);
}

function notUtilIsDeepStrict(a, b) {
  assert.strictEqual(util.isDeepStrictEqual(a, b), false);
  assert.strictEqual(util.isDeepStrictEqual(b, a), false);
}

// Handle boxed primitives
{
  const boxedString = new String('test');
  const boxedSymbol = Object(Symbol());
  notUtilIsDeepStrict(new Boolean(true), Object(false));
  notUtilIsDeepStrict(Object(true), new Number(1));
  notUtilIsDeepStrict(new Number(2), new Number(1));
  notUtilIsDeepStrict(boxedSymbol, Object(Symbol()));
  notUtilIsDeepStrict(boxedSymbol, {});
  utilIsDeepStrict(boxedSymbol, boxedSymbol);
  utilIsDeepStrict(Object(true), Object(true));
  utilIsDeepStrict(Object(2), Object(2));
  utilIsDeepStrict(boxedString, Object('test'));
  boxedString.slow = true;
  notUtilIsDeepStrict(boxedString, Object('test'));
  boxedSymbol.slow = true;
  notUtilIsDeepStrict(boxedSymbol, {});
  utilIsDeepStrict(Object(BigInt(1)), Object(BigInt(1)));
  notUtilIsDeepStrict(Object(BigInt(1)), Object(BigInt(2)));

  const booleanish = new Boolean(true);
  Object.defineProperty(booleanish, Symbol.toStringTag, { value: 'String' });
  Object.setPrototypeOf(booleanish, String.prototype);
  notUtilIsDeepStrict(booleanish, new String('true'));

  const numberish = new Number(42);
  Object.defineProperty(numberish, Symbol.toStringTag, { value: 'String' });
  Object.setPrototypeOf(numberish, String.prototype);
  notUtilIsDeepStrict(numberish, new String('42'));

  const stringish = new String('0');
  Object.defineProperty(stringish, Symbol.toStringTag, { value: 'Number' });
  Object.setPrototypeOf(stringish, Number.prototype);
  notUtilIsDeepStrict(stringish, new Number(0));

  const bigintish = new Object(BigInt(42));
  Object.defineProperty(bigintish, Symbol.toStringTag, { value: 'String' });
  Object.setPrototypeOf(bigintish, String.prototype);
  notUtilIsDeepStrict(bigintish, new String('42'));

  const symbolish = new Object(Symbol('fhqwhgads'));
  Object.defineProperty(symbolish, Symbol.toStringTag, { value: 'String' });
  Object.setPrototypeOf(symbolish, String.prototype);
  notUtilIsDeepStrict(symbolish, new String('fhqwhgads'));
}

// Handle symbols (enumerable only)
{
  const symbol1 = Symbol();
  const obj1 = { [symbol1]: 1 };
  const obj2 = { [symbol1]: 1 };
  const obj3 = { [Symbol()]: 1 };
  const obj4 = { };
  // Add a non enumerable symbol as well. It is going to be ignored!
  Object.defineProperty(obj2, Symbol(), { value: 1 });
  Object.defineProperty(obj4, symbol1, { value: 1 });
  notUtilIsDeepStrict(obj1, obj3);
  utilIsDeepStrict(obj1, obj2);
  notUtilIsDeepStrict(obj1, obj4);
  // TypedArrays have a fast path. Test for this as well.
  const a = new Uint8Array(4);
  const b = new Uint8Array(4);
  a[symbol1] = true;
  b[symbol1] = false;
  notUtilIsDeepStrict(a, b);
  b[symbol1] = true;
  utilIsDeepStrict(a, b);
  // The same as TypedArrays is valid for boxed primitives
  const boxedStringA = new String('test');
  const boxedStringB = new String('test');
  boxedStringA[symbol1] = true;
  notUtilIsDeepStrict(boxedStringA, boxedStringB);
  boxedStringA[symbol1] = true;
  utilIsDeepStrict(a, b);
}

// Handle `skipPrototype` for isDeepStrictEqual
{
  test('util.isDeepStrictEqual with skipPrototype', () => {
    function ClassA(value) { this.value = value; }

    function ClassB(value) { this.value = value; }

    const objA = new ClassA(42);
    const objB = new ClassB(42);

    assert.strictEqual(util.isDeepStrictEqual(objA, objB), false);
    assert.strictEqual(util.isDeepStrictEqual(objA, objB, true), true);

    const objC = new ClassB(99);
    assert.strictEqual(util.isDeepStrictEqual(objA, objC, true), false);

    const nestedA = { obj: new ClassA('test'), num: 123 };
    const nestedB = { obj: new ClassB('test'), num: 123 };

    assert.strictEqual(util.isDeepStrictEqual(nestedA, nestedB), false);
    assert.strictEqual(util.isDeepStrictEqual(nestedA, nestedB, true), true);

    const uint8Array = new Uint8Array([1, 2, 3]);
    const buffer = Buffer.from([1, 2, 3]);

    assert.strictEqual(util.isDeepStrictEqual(uint8Array, buffer), false);
    assert.strictEqual(util.isDeepStrictEqual(uint8Array, buffer, true), true);
  });

  test('util.isDeepStrictEqual skipPrototype with complex scenarios', () => {
    class Parent { constructor(x) { this.x = x; } }
    class Child extends Parent { constructor(x, y) { super(x); this.y = y; } }

    function LegacyParent(x) { this.x = x; }

    function LegacyChild(x, y) { this.x = x; this.y = y; }

    const modernParent = new Parent(1);
    const legacyParent = new LegacyParent(1);

    assert.strictEqual(util.isDeepStrictEqual(modernParent, legacyParent), false);
    assert.strictEqual(util.isDeepStrictEqual(modernParent, legacyParent, true), true);

    const modern = new Child(1, 2);
    const legacy = new LegacyChild(1, 2);

    assert.strictEqual(util.isDeepStrictEqual(modern, legacy), false);
    assert.strictEqual(util.isDeepStrictEqual(modern, legacy, true), true);

    const literal = { name: 'test', values: [1, 2, 3] };
    function Constructor(name, values) { this.name = name; this.values = values; }
    const constructed = new Constructor('test', [1, 2, 3]);

    assert.strictEqual(util.isDeepStrictEqual(literal, constructed), false);
    assert.strictEqual(util.isDeepStrictEqual(literal, constructed, true), true);

    assert.strictEqual(util.isDeepStrictEqual(literal, constructed, false), false);
    assert.strictEqual(util.isDeepStrictEqual(literal, constructed, null), false);
    assert.strictEqual(util.isDeepStrictEqual(literal, constructed, undefined), false);
  });
}
