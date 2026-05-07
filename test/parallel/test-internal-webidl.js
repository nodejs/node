// Flags: --expose-internals
'use strict';

require('../common');
const assert = require('assert');
const vm = require('vm');
const webidl = require('internal/webidl');

const { converters } = webidl;
const opts = {
  __proto__: null,
  prefix: 'Prefix',
  context: 'Context',
};
function createGrowableSharedArrayBufferView() {
  const buffer = new SharedArrayBuffer(4, { maxByteLength: 8 });
  const view = new Uint8Array(buffer);
  assert.strictEqual(view.buffer.growable, true);
  return view;
}

function assertInvalidArgType(fn) {
  assert.throws(fn, {
    name: 'TypeError',
    code: 'ERR_INVALID_ARG_TYPE',
  });
}

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

assert.strictEqual(webidl.type(undefined), 'Undefined');
assert.strictEqual(webidl.type(null), 'Null');
assert.strictEqual(webidl.type(false), 'Boolean');
assert.strictEqual(webidl.type(''), 'String');
assert.strictEqual(webidl.type(Symbol()), 'Symbol');
assert.strictEqual(webidl.type(0), 'Number');
assert.strictEqual(webidl.type(0n), 'BigInt');
assert.strictEqual(webidl.type({}), 'Object');
assert.strictEqual(webidl.type(function fn() {}), 'Object');

assert.strictEqual(converters.boolean(0), false);
assert.strictEqual(converters.boolean('false'), true);
{
  function fn() {}
  assert.strictEqual(converters.object(fn), fn);
}
assert.throws(() => converters.object(null, opts), {
  name: 'TypeError',
  code: 'ERR_INVALID_ARG_TYPE',
  message: 'Prefix: Context is not an object.',
});

assert.strictEqual(converters.DOMString(null), 'null');
assert.throws(() => converters.DOMString(Symbol(), opts), {
  name: 'TypeError',
  code: 'ERR_INVALID_ARG_TYPE',
  message: 'Prefix: Context is a Symbol and cannot be converted to a string.',
});

for (const value of [
  Object(Symbol()),
  { toString() { return Symbol(); } },
  { toString() { return {}; }, valueOf() { return Symbol(); } },
  { [Symbol.toPrimitive]() { return Symbol(); } },
]) {
  assertPlainTypeError(() => converters.DOMString(value, opts));
}
assert.strictEqual(converters.DOMString(1n), '1');
assert.strictEqual(converters.DOMString(Object(1n)), '1');
assert.strictEqual(converters.DOMString({
  toString() { return {}; },
  valueOf() { return 1n; },
}), '1');
assert.strictEqual(converters.DOMString({
  [Symbol.toPrimitive]() { return 1n; },
}), '1');
{
  const value = Object(Symbol());
  Object.defineProperty(value, Symbol.toPrimitive, {
    __proto__: null,
    value() { return 'symbol object'; },
  });
  assert.strictEqual(converters.DOMString(value), 'symbol object');
}

{
  const calls = [];
  const value = {
    [Symbol.toPrimitive](hint) {
      calls.push(hint);
      return 7;
    },
    toString() {
      calls.push('toString');
      return '1';
    },
    valueOf() {
      calls.push('valueOf');
      return 1;
    },
  };

  assert.strictEqual(converters.DOMString(value), '7');
  assert.deepStrictEqual(calls, ['string']);
}

for (const { value, expected } of [
  {
    value: {
      [Symbol.toPrimitive]: undefined,
      toString() { return 'eight'; },
    },
    expected: 'eight',
  },
  {
    value: {
      [Symbol.toPrimitive]: null,
      toString() { return 'nine'; },
    },
    expected: 'nine',
  },
]) {
  assert.strictEqual(converters.DOMString(value), expected);
}

{
  const calls = [];
  const value = {
    toString: 1,
    valueOf() {
      calls.push('valueOf');
      return 10;
    },
  };

  assert.strictEqual(converters.DOMString(value), '10');
  assert.deepStrictEqual(calls, ['valueOf']);
}

{
  const calls = [];
  const value = {
    toString() {
      calls.push('toString');
      return {};
    },
    valueOf() {
      calls.push('valueOf');
      return 11;
    },
  };

  assert.strictEqual(converters.DOMString(value), '11');
  assert.deepStrictEqual(calls, ['toString', 'valueOf']);
}

for (const value of [
  { [Symbol.toPrimitive]: 1 },
  { [Symbol.toPrimitive]() { return {}; } },
  {
    toString() { return {}; },
    valueOf() { return {}; },
  },
]) {
  assertPlainTypeError(() => converters.DOMString(value, opts));
}

{
  const sentinel = new TypeError('sentinel');
  assertSameError(() => converters.DOMString({
    get [Symbol.toPrimitive]() {
      throw sentinel;
    },
  }), sentinel);
}

{
  const sentinel = new TypeError('sentinel');
  assertSameError(() => converters.DOMString({
    get toString() {
      throw sentinel;
    },
    valueOf() {
      return 1;
    },
  }), sentinel);
}

{
  const sentinel = new TypeError('sentinel');
  assertSameError(() => converters.DOMString({
    toString() {
      throw sentinel;
    },
    valueOf() {
      return 1;
    },
  }), sentinel);
}

{
  assert.strictEqual(converters.octet(-1), 255);
  assert.strictEqual(converters['unsigned short'](-1), 0xFFFF);
  assert.strictEqual(converters['unsigned long'](-1), 0xFFFF_FFFF);
  assert.strictEqual(converters['long long'](-1), -1);
  assert.strictEqual(converters.octet(2.5, {
    __proto__: null,
    clamp: true,
  }), 2);
  assert.throws(() => converters.octet(256, {
    __proto__: null,
    ...opts,
    enforceRange: true,
  }), {
    name: 'TypeError',
    code: 'ERR_OUT_OF_RANGE',
    message: 'Prefix: Context is outside the expected range of 0 to 255.',
  });
}

{
  const converter = webidl.createEnumConverter('Example', ['one', 'two']);

  assert.strictEqual(converter('one'), 'one');
  assert.throws(() => converter(Symbol(), opts), {
    name: 'TypeError',
    code: 'ERR_INVALID_ARG_TYPE',
    message: 'Prefix: Context is a Symbol and cannot be converted to a string.',
  });
  assert.throws(() => converter('three', opts), {
    name: 'TypeError',
    code: 'ERR_INVALID_ARG_VALUE',
    message: "Prefix: Context 'three' is not a valid enum value " +
      'of type Example.',
  });
}

assert.throws(() => webidl.requiredArguments(1, 2, opts), {
  name: 'TypeError',
  code: 'ERR_MISSING_ARGS',
  message: 'Prefix: 2 arguments required, but only 1 present.',
});

{
  const ab = new ArrayBuffer(8, { maxByteLength: 16 });
  const view = new Uint8Array(ab);
  const dataView = new DataView(ab);

  assert.strictEqual(ab.resizable, true);
  assertInvalidArgType(() => converters.BufferSource(ab));
  assertInvalidArgType(() => converters.BufferSource(view));
  assertInvalidArgType(() => converters.BufferSource(dataView));
  assertInvalidArgType(() => converters.Uint8Array(view));

  const disallowResizable = {
    __proto__: null,
    allowResizable: false,
  };
  assertInvalidArgType(() => converters.BufferSource(ab, disallowResizable));
  assertInvalidArgType(() => converters.BufferSource(view, disallowResizable));
  assertInvalidArgType(() => converters.BufferSource(dataView,
                                                     disallowResizable));

  const allowResizable = {
    __proto__: null,
    allowResizable: true,
  };
  assert.strictEqual(converters.BufferSource(ab, allowResizable), ab);
  assert.strictEqual(converters.BufferSource(view, allowResizable), view);
  assert.strictEqual(converters.BufferSource(dataView, allowResizable),
                     dataView);
  assert.strictEqual(converters.Uint8Array(view, allowResizable), view);
}

{
  const ab = new ArrayBuffer(8);
  const view = new Uint8Array(ab);
  const dataView = new DataView(ab);

  structuredClone(ab, { transfer: [ab] });
  assert.strictEqual(ab.detached, true);
  assert.strictEqual(converters.BufferSource(ab), ab);
  assert.strictEqual(converters.BufferSource(view), view);
  assert.strictEqual(converters.BufferSource(dataView), dataView);
}

{
  const ab = new ArrayBuffer(8, { maxByteLength: 16 });
  const view = new Uint8Array(ab);
  const dataView = new DataView(ab);
  const allowResizable = {
    __proto__: null,
    allowResizable: true,
  };

  structuredClone(ab, { transfer: [ab] });
  assert.strictEqual(ab.detached, true);
  assert.strictEqual(ab.resizable, true);
  assertInvalidArgType(() => converters.BufferSource(ab));
  assertInvalidArgType(() => converters.BufferSource(view));
  assertInvalidArgType(() => converters.BufferSource(dataView));
  assertInvalidArgType(() => converters.Uint8Array(view));

  const disallowResizable = {
    __proto__: null,
    allowResizable: false,
  };
  assertInvalidArgType(() => converters.BufferSource(ab, disallowResizable));
  assertInvalidArgType(() => converters.BufferSource(view, disallowResizable));
  assertInvalidArgType(() => converters.BufferSource(dataView,
                                                     disallowResizable));

  assert.strictEqual(converters.BufferSource(ab, allowResizable), ab);
  assert.strictEqual(converters.BufferSource(view, allowResizable), view);
  assert.strictEqual(converters.BufferSource(dataView, allowResizable),
                     dataView);
  assert.strictEqual(converters.Uint8Array(view, allowResizable), view);
}

{
  const ab = new ArrayBuffer(8, { maxByteLength: 16 });
  const view = new Uint8Array(ab);

  assert.strictEqual(converters.BufferSource(ab, {
    __proto__: null,
    allowResizable: true,
  }), ab);
  assert.strictEqual(converters.BufferSource(view, {
    __proto__: null,
    allowResizable: true,
  }), view);

  ab.resize(4);
  assert.strictEqual(ab.byteLength, 4);
  assert.strictEqual(view.byteLength, 4);

  ab.resize(12);
  assert.strictEqual(ab.byteLength, 12);
  assert.strictEqual(view.byteLength, 12);
}

{
  const converter = webidl.createDictionaryConverter('Example', [
    {
      key: 'value',
      converter: converters.DOMString,
      required: true,
    },
  ]);

  const idlDict = converter({ value: 1 });
  assert.deepStrictEqual(idlDict, { __proto__: null, value: '1' });

  assert.throws(() => converter({}, opts), {
    name: 'TypeError',
    code: 'ERR_MISSING_OPTION',
    message: "Prefix: Context cannot be converted to 'Example' because " +
      "'value' is required in 'Example'.",
  });
}

{
  const calls = [];
  const converter = webidl.createDictionaryConverter('Derived', [
    [
      {
        key: 'zBase',
        converter(value) {
          calls.push(`base z:${value}`);
          return value;
        },
      },
      {
        key: 'aBase',
        converter(value) {
          calls.push(`base a:${value}`);
          return value;
        },
      },
    ],
    [
      {
        key: 'zDerived',
        converter(value) {
          calls.push(`derived z:${value}`);
          return value;
        },
      },
      {
        key: 'aDerived',
        converter(value) {
          calls.push(`derived a:${value}`);
          return value;
        },
      },
    ],
  ]);

  assert.deepStrictEqual(converter({
    zBase: 1,
    aBase: 2,
    zDerived: 3,
    aDerived: 4,
  }), {
    __proto__: null,
    aBase: 2,
    zBase: 1,
    aDerived: 4,
    zDerived: 3,
  });
  assert.deepStrictEqual(calls, [
    'base a:2',
    'base z:1',
    'derived a:4',
    'derived z:3',
  ]);
}

{
  const converter = webidl.createDictionaryConverter('Example', [
    {
      key: 'same',
      converter(value) {
        return `first:${value}`;
      },
    },
    {
      key: 'same',
      converter(value) {
        return `second:${value}`;
      },
    },
  ]);

  assert.deepStrictEqual(converter({ same: 1 }), {
    __proto__: null,
    same: 'second:1',
  });
}

{
  const converter = converters['sequence<DOMString>'];
  assert.deepStrictEqual(converter([1, 2]), ['1', '2']);

  assert.throws(() => converter([Symbol()]), {
    name: 'TypeError',
    code: 'ERR_INVALID_ARG_TYPE',
    message: 'Value[0] is a Symbol and cannot be converted to a string.',
  });

  assert.throws(() => converter({ [Symbol.iterator]: 1 }, opts), {
    name: 'TypeError',
    code: 'ERR_INVALID_ARG_TYPE',
    message: 'Prefix: Context cannot be converted to sequence.',
  });

  assert.throws(() => converter({
    [Symbol.iterator]() {
      return {};
    },
  }, opts), {
    name: 'TypeError',
    code: 'ERR_INVALID_ARG_TYPE',
    message: 'Prefix: Context cannot be converted to sequence.',
  });

  assert.throws(() => converter({
    [Symbol.iterator]() {
      return {
        next() {
          return null;
        },
      };
    },
  }, opts), {
    name: 'TypeError',
    code: 'ERR_INVALID_ARG_TYPE',
    message: 'Prefix: Context cannot be converted to sequence.',
  });

  assert.throws(() => converter([Symbol()], opts), {
    name: 'TypeError',
    code: 'ERR_INVALID_ARG_TYPE',
    message: 'Prefix: Context[0] is a Symbol and cannot be converted ' +
      'to a string.',
  });

  assert.deepStrictEqual(converter({
    [Symbol.iterator]() {
      return {
        next() {
          return { done: 1, value: Symbol() };
        },
      };
    },
  }), []);
}

{
  class Example {}
  const converter = webidl.createInterfaceConverter(
    'Example',
    Example.prototype);
  const example = new Example();

  assert.strictEqual(converter(example), example);
  assert.throws(() => converter({}, opts), {
    name: 'TypeError',
    code: 'ERR_INVALID_ARG_TYPE',
    message: 'Prefix: Context is not of type Example.',
  });
}

{
  const context = vm.createContext();
  const view = vm.runInContext(
    'new Uint8Array(new ArrayBuffer(0))',
    context,
  );
  assert.strictEqual(converters.Uint8Array(view), view);

  for (const value of [
    new ArrayBuffer(0),
    new SharedArrayBuffer(0),
    new DataView(new ArrayBuffer(0)),
    new Int8Array(0),
    Object.create(Uint8Array.prototype, {
      [Symbol.toStringTag]: { value: 'Uint8Array' },
    }),
  ]) {
    assertInvalidArgType(() => converters.Uint8Array(value));
  }
}

{
  const sab = new SharedArrayBuffer(4);
  const view = new Uint8Array(sab);

  assertInvalidArgType(() => converters.BufferSource(sab));
  assertInvalidArgType(() => converters.BufferSource(view));
  assertInvalidArgType(() => converters.Uint8Array(view));

  const allowShared = {
    __proto__: null,
    allowShared: true,
  };
  assertInvalidArgType(() => converters.BufferSource(sab, allowShared));
  assertInvalidArgType(() => converters.BufferSource(view, allowShared));
  assert.strictEqual(converters.AllowSharedBufferSource(sab), sab);
  assert.strictEqual(converters.AllowSharedBufferSource(view), view);
  assert.strictEqual(converters.Uint8Array(view, allowShared), view);

  const growableView = createGrowableSharedArrayBufferView();

  assertInvalidArgType(() => converters.BufferSource(growableView, {
    __proto__: null,
    allowShared: true,
    allowResizable: true,
  }));
  assert.throws(() => converters.AllowSharedBufferSource(growableView.buffer, {
    __proto__: null,
    ...opts,
  }), {
    name: 'TypeError',
    code: 'ERR_INVALID_ARG_TYPE',
    message: 'Prefix: Context is backed by a growable ' +
      'SharedArrayBuffer, which is not allowed.',
  });
  assert.throws(() => converters.AllowSharedBufferSource(growableView, {
    __proto__: null,
    ...opts,
    allowResizable: false,
  }), {
    name: 'TypeError',
    code: 'ERR_INVALID_ARG_TYPE',
    message: 'Prefix: Context is backed by a growable ' +
      'SharedArrayBuffer, which is not allowed.',
  });
  assert.throws(() => converters.Uint8Array(growableView, {
    __proto__: null,
    ...opts,
    allowShared: true,
  }), {
    name: 'TypeError',
    code: 'ERR_INVALID_ARG_TYPE',
    message: 'Prefix: Context is backed by a growable ' +
      'SharedArrayBuffer, which is not allowed.',
  });
  assert.strictEqual(converters.AllowSharedBufferSource(growableView.buffer, {
    __proto__: null,
    allowResizable: true,
  }), growableView.buffer);
  assert.strictEqual(converters.AllowSharedBufferSource(growableView, {
    __proto__: null,
    allowResizable: true,
  }), growableView);
  assert.strictEqual(converters.Uint8Array(growableView, {
    __proto__: null,
    allowShared: true,
    allowResizable: true,
  }), growableView);
}
