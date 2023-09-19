'use strict';

// Confirm functionality of `util.isDeepStrictEqual()`.

require('../common');

const assert = require('assert');
const util = require('util');

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
