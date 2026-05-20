// Flags: --expose-internals
'use strict';

require('../common');
const assert = require('assert');
const { convertToInt } = require('internal/webidl');

function assertPlainTypeError(fn) {
  assert.throws(fn, (err) => {
    assert(err instanceof TypeError);
    assert.strictEqual(err.name, 'TypeError');
    assert.strictEqual(err.code, undefined);
    return true;
  });
}

function assertSameError(fn, expected) {
  assert.throws(fn, (err) => {
    assert.strictEqual(err, expected);
    return true;
  });
}

// https://webidl.spec.whatwg.org/#abstract-opdef-converttoint
const two63 = 2 ** 63;
const two64 = 2 ** 64;

assert.strictEqual(convertToInt(0, 64), 0);
assert.strictEqual(convertToInt(1, 64), 1);
assert.strictEqual(convertToInt(-0.5, 64), 0);
assert.strictEqual(convertToInt(-0.5, 64, 'signed'), 0);
assert.strictEqual(convertToInt(-1.5, 64, 'signed'), -1);
assert.strictEqual(convertToInt(2 ** 12 + 1, 12), 1);
assert.strictEqual(convertToInt(-1, 12), 2 ** 12 - 1);
assert.strictEqual(convertToInt(2 ** 11, 12, 'signed'), -(2 ** 11));
assert.strictEqual(convertToInt(-1, 12, 'signed'), -1);

{
  const options = {
    get enforceRange() {
      throw new Error('enforceRange should not be read');
    },
    get clamp() {
      throw new Error('clamp should not be read');
    },
  };

  assert.strictEqual(convertToInt(7, 8, 'unsigned', options), 7);
}

{
  const opts = {
    __proto__: null,
    prefix: 'Prefix',
    context: 'Context',
  };

  assert.throws(() => convertToInt(1n, 8, 'unsigned', opts), {
    name: 'TypeError',
    code: 'ERR_INVALID_ARG_TYPE',
    message: 'Prefix: Context is a BigInt and cannot be converted ' +
      'to a number.',
  });

  assert.throws(() => convertToInt(Symbol(), 8, 'unsigned', opts), {
    name: 'TypeError',
    code: 'ERR_INVALID_ARG_TYPE',
    message: 'Prefix: Context is a Symbol and cannot be converted ' +
      'to a number.',
  });

  for (const value of [
    Object(1n),
    { valueOf() { return 1n; } },
    { valueOf() { return {}; }, toString() { return 1n; } },
    { [Symbol.toPrimitive]() { return 1n; } },
    Object(Symbol()),
    { valueOf() { return Symbol(); } },
    { valueOf() { return {}; }, toString() { return Symbol(); } },
    { [Symbol.toPrimitive]() { return Symbol(); } },
  ]) {
    assertPlainTypeError(() => convertToInt(value, 8, 'unsigned', opts));
  }

  assert.strictEqual(convertToInt({
    valueOf() { return {}; },
    toString() { return 7; },
  }, 8), 7);

  {
    const value = Object(1n);
    value.valueOf = () => 7;
    assert.strictEqual(convertToInt(value, 8), 7);
  }

  {
    const calls = [];
    const value = {
      [Symbol.toPrimitive](hint) {
        calls.push(hint);
        return '7';
      },
      valueOf() {
        calls.push('valueOf');
        return 1;
      },
      toString() {
        calls.push('toString');
        return '1';
      },
    };

    assert.strictEqual(convertToInt(value, 8), 7);
    assert.deepStrictEqual(calls, ['number']);
  }

  for (const { value, expected } of [
    {
      value: {
        [Symbol.toPrimitive]: undefined,
        valueOf() { return 8; },
      },
      expected: 8,
    },
    {
      value: {
        [Symbol.toPrimitive]: null,
        valueOf() { return 9; },
      },
      expected: 9,
    },
  ]) {
    assert.strictEqual(convertToInt(value, 8), expected);
  }

  {
    const calls = [];
    const value = {
      valueOf: 1,
      toString() {
        calls.push('toString');
        return '10';
      },
    };

    assert.strictEqual(convertToInt(value, 8), 10);
    assert.deepStrictEqual(calls, ['toString']);
  }

  {
    const calls = [];
    const value = {
      valueOf() {
        calls.push('valueOf');
        return {};
      },
      toString() {
        calls.push('toString');
        return '11';
      },
    };

    assert.strictEqual(convertToInt(value, 8), 11);
    assert.deepStrictEqual(calls, ['valueOf', 'toString']);
  }

  for (const value of [
    { [Symbol.toPrimitive]: 1 },
    { [Symbol.toPrimitive]() { return {}; } },
    {
      valueOf() { return {}; },
      toString() { return {}; },
    },
  ]) {
    assertPlainTypeError(() => convertToInt(value, 8, 'unsigned', opts));
  }

  {
    const sentinel = new TypeError('sentinel');
    assertSameError(() => convertToInt({
      get [Symbol.toPrimitive]() {
        throw sentinel;
      },
    }, 8), sentinel);
  }

  {
    const sentinel = new TypeError('sentinel');
    assertSameError(() => convertToInt({
      get valueOf() {
        throw sentinel;
      },
      toString() {
        return 1;
      },
    }, 8), sentinel);
  }

  {
    const sentinel = new TypeError('sentinel');
    assertSameError(() => convertToInt({
      valueOf() {
        throw sentinel;
      },
      toString() {
        return 1;
      },
    }, 8), sentinel);
  }
}

// EnforceRange
const nonFiniteValues = [NaN, Infinity, -Infinity];
for (const value of nonFiniteValues) {
  assert.throws(() => convertToInt(value, 64, 'unsigned', {
    enforceRange: true,
  }), {
    name: 'TypeError',
    code: 'ERR_INVALID_ARG_TYPE',
  });
}

assert.strictEqual(convertToInt(-0.8, 64, 'unsigned', {
  enforceRange: true,
}), 0);
assert.strictEqual(convertToInt(-0.8, 64, 'signed', {
  enforceRange: true,
}), 0);
assert.strictEqual(convertToInt(Number.MAX_SAFE_INTEGER, 64, 'signed', {
  enforceRange: true,
}), Number.MAX_SAFE_INTEGER);
assert.strictEqual(convertToInt(Number.MIN_SAFE_INTEGER, 64, 'signed', {
  enforceRange: true,
}), Number.MIN_SAFE_INTEGER);
assert.strictEqual(convertToInt(-0.5, 8, 'unsigned', {
  enforceRange: true,
}), 0);
assert.strictEqual(convertToInt(255.5, 8, 'unsigned', {
  enforceRange: true,
}), 255);

const outOfRangeValues = [2 ** 53, -(2 ** 53)];
for (const value of outOfRangeValues) {
  assert.throws(() => convertToInt(value, 64, 'unsigned', {
    enforceRange: true,
  }), {
    name: 'TypeError',
    code: 'ERR_OUT_OF_RANGE',
  });
}
assert.throws(() => convertToInt(Number.MAX_SAFE_INTEGER + 1, 64, 'signed', {
  enforceRange: true,
}), {
  name: 'TypeError',
  code: 'ERR_OUT_OF_RANGE',
});
assert.throws(() => convertToInt(Number.MIN_SAFE_INTEGER - 1, 64, 'signed', {
  enforceRange: true,
}), {
  name: 'TypeError',
  code: 'ERR_OUT_OF_RANGE',
});
assert.throws(() => convertToInt(256, 8, 'unsigned', {
  enforceRange: true,
}), {
  name: 'TypeError',
  code: 'ERR_OUT_OF_RANGE',
});

{
  const calls = [];
  const options = {
    get enforceRange() {
      calls.push('enforceRange');
      return false;
    },
    get clamp() {
      calls.push('clamp');
      return true;
    },
  };

  assert.strictEqual(convertToInt(256, 8, 'unsigned', options), 255);
  assert.deepStrictEqual(calls, ['enforceRange', 'clamp']);
}

{
  const calls = [];
  const options = {
    get enforceRange() {
      calls.push('enforceRange');
      return false;
    },
    get clamp() {
      calls.push('clamp');
      return true;
    },
  };

  assert.strictEqual(convertToInt({ valueOf: () => 2.5 }, 8, 'unsigned', options), 2);
  assert.deepStrictEqual(calls, ['enforceRange', 'clamp']);
}

// Out of range: clamp
assert.strictEqual(convertToInt(NaN, 64, 'unsigned', { clamp: true }), 0);
assert.strictEqual(convertToInt(Infinity, 64, 'unsigned', { clamp: true }), Number.MAX_SAFE_INTEGER);
assert.strictEqual(convertToInt(-Infinity, 64, 'unsigned', { clamp: true }), 0);
assert.strictEqual(convertToInt(-Infinity, 64, 'signed', { clamp: true }), Number.MIN_SAFE_INTEGER);
assert.strictEqual(convertToInt(0x1_0000, 16, 'unsigned', { clamp: true }), 0xFFFF);
assert.strictEqual(convertToInt(0x1_0000_0000, 32, 'unsigned', { clamp: true }), 0xFFFF_FFFF);
assert.strictEqual(convertToInt(0xFFFF_FFFF, 32, 'unsigned', { clamp: true }), 0xFFFF_FFFF);
assert.strictEqual(convertToInt(0x8000, 16, 'signed', { clamp: true }), 0x7FFF);
assert.strictEqual(convertToInt(-0x8001, 16, 'signed', { clamp: true }), -0x8000);
assert.strictEqual(convertToInt(0x8000_0000, 32, 'signed', { clamp: true }), 0x7FFF_FFFF);
assert.strictEqual(convertToInt(0xFFFF_FFFF, 32, 'signed', { clamp: true }), 0x7FFF_FFFF);
assert.strictEqual(convertToInt(0.5, 64, 'unsigned', { clamp: true }), 0);
assert.strictEqual(convertToInt(0.8, 64, 'unsigned', { clamp: true }), 1);
assert.strictEqual(convertToInt(1.5, 64, 'unsigned', { clamp: true }), 2);
assert.strictEqual(convertToInt(-0.5, 64, 'unsigned', { clamp: true }), 0);
assert.strictEqual(convertToInt(-0.5, 64, 'signed', { clamp: true }), 0);
assert.strictEqual(convertToInt(-0.8, 64, 'signed', { clamp: true }), -1);
assert.strictEqual(convertToInt(-1.5, 64, 'signed', { clamp: true }), -2);

// Out of range, step 8.
assert.strictEqual(convertToInt(NaN, 64), 0);
assert.strictEqual(convertToInt(Infinity, 64), 0);
assert.strictEqual(convertToInt(-Infinity, 64), 0);
assert.strictEqual(convertToInt(-3, 8), 253);
assert.strictEqual(convertToInt(-3, 8), new Uint8Array([-3])[0]);
assert.strictEqual(convertToInt(0x1_0000, 16), 0);
assert.strictEqual(convertToInt(0x1_0001, 16), 1);
assert.strictEqual(convertToInt(-1, 16), 0xFFFF);
assert.strictEqual(convertToInt(-1, 16), new Uint16Array([-1])[0]);
assert.strictEqual(convertToInt(0x1_0000_0000, 32), 0);
assert.strictEqual(convertToInt(0x1_0000_0001, 32), 1);
assert.strictEqual(convertToInt(0xFFFF_FFFF, 32), 0xFFFF_FFFF);
assert.strictEqual(convertToInt(-1, 32), 0xFFFF_FFFF);
assert.strictEqual(convertToInt(-1, 32), new Uint32Array([-1])[0]);
assert.strictEqual(convertToInt(-8192, 64), 2 ** 64 - 8192);
assert.strictEqual(convertToInt(two63, 64), two63);
assert.strictEqual(convertToInt(two64, 64), 0);
assert.strictEqual(convertToInt(two64 + 8192, 64), 8192);
assert.strictEqual(convertToInt(-(two64 + 2 ** 12), 64),
                   two64 - 2 ** 12);

// Out of range, step 11.
assert.strictEqual(convertToInt(0x8000, 16, 'signed'), -0x8000);
assert.strictEqual(convertToInt(0xFFFF, 16, 'signed'), -1);
assert.strictEqual(convertToInt(-0x8001, 16, 'signed'), 0x7FFF);
assert.strictEqual(convertToInt(-0x8001, 16, 'signed'), new Int16Array([-0x8001])[0]);
assert.strictEqual(convertToInt(0x8000_0000, 32, 'signed'), -0x8000_0000);
assert.strictEqual(convertToInt(0xFFF_FFFF, 32, 'signed'), 0xFFF_FFFF);
assert.strictEqual(convertToInt(-200, 8, 'signed'), 56);
assert.strictEqual(convertToInt(-200, 8, 'signed'), new Int8Array([-200])[0]);
assert.strictEqual(convertToInt(-8192, 64, 'signed'), -8192);
assert.strictEqual(convertToInt(-8193, 64, 'signed'), -8193);
assert.strictEqual(convertToInt(two63, 64, 'signed'), -two63);
assert.strictEqual(convertToInt(two63 + 2 ** 11, 64, 'signed'),
                   -two63 + 2 ** 11);
assert.strictEqual(convertToInt(two64 + 8192, 64, 'signed'), 8192);
assert.strictEqual(convertToInt(-(two64 + 2 ** 12), 64, 'signed'),
                   -(2 ** 12));
