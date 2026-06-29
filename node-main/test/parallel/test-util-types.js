// Flags: --experimental-vm-modules --expose-internals --allow-natives-syntax
'use strict';
const common = require('../common');
const assert = require('assert');
const { types, inspect } = require('util');
const vm = require('vm');
const { internalBinding } = require('internal/test/binding');
const { JSStream } = internalBinding('js_stream');

const external = (new JSStream())._externalStream;

for (const [ value, _method ] of [
  [ external, 'isExternal' ],
  [ new Date() ],
  [ (function() { return arguments; })(), 'isArgumentsObject' ],
  [ new Boolean(), 'isBooleanObject' ],
  [ new Number(), 'isNumberObject' ],
  [ new String(), 'isStringObject' ],
  [ Object(Symbol()), 'isSymbolObject' ],
  [ Object(BigInt(0)), 'isBigIntObject' ],
  [ new Error(), 'isNativeError' ],
  [ new RegExp() ],
  [ async function() {}, 'isAsyncFunction' ],
  [ function*() {}, 'isGeneratorFunction' ],
  [ (function*() {})(), 'isGeneratorObject' ],
  [ Promise.resolve() ],
  [ new Map() ],
  [ new Set() ],
  [ (new Map())[Symbol.iterator](), 'isMapIterator' ],
  [ (new Set())[Symbol.iterator](), 'isSetIterator' ],
  [ new WeakMap() ],
  [ new WeakSet() ],
  [ new ArrayBuffer() ],
  [ new Uint8Array() ],
  [ new Uint8ClampedArray() ],
  [ new Uint16Array() ],
  [ new Uint32Array() ],
  [ new Int8Array() ],
  [ new Int16Array() ],
  [ new Int32Array() ],
  [ new Float16Array() ],
  [ new Float32Array() ],
  [ new Float64Array() ],
  [ new BigInt64Array() ],
  [ new BigUint64Array() ],
  [ Object.defineProperty(new Uint8Array(),
                          Symbol.toStringTag,
                          { value: 'foo' }) ],
  [ new DataView(new ArrayBuffer()) ],
  [ new SharedArrayBuffer() ],
  [ new Proxy({}, {}), 'isProxy' ],
]) {
  const method = _method || `is${value.constructor.name}`;
  assert(method in types, `Missing ${method} for ${inspect(value)}`);
  assert(types[method](value), `Want ${inspect(value)} to match ${method}`);

  for (const key of Object.keys(types)) {
    if ((types.isArrayBufferView(value) ||
         types.isAnyArrayBuffer(value)) && key.includes('Array') ||
         key === 'isBoxedPrimitive') {
      continue;
    }

    assert.strictEqual(types[key](value),
                       key === method,
                       `${inspect(value)}: ${key}, ` +
                       `${method}, ${types[key](value)}`);
  }
}

// Check boxed primitives.
[
  new Boolean(),
  new Number(),
  new String(),
  Object(Symbol()),
  Object(BigInt(0)),
].forEach((entry) => assert(types.isBoxedPrimitive(entry)));

{
  assert(!types.isUint8Array({ [Symbol.toStringTag]: 'Uint8Array' }));
  assert(types.isUint8Array(vm.runInNewContext('new Uint8Array')));

  assert(!types.isUint8ClampedArray({
    [Symbol.toStringTag]: 'Uint8ClampedArray'
  }));
  assert(types.isUint8ClampedArray(
    vm.runInNewContext('new Uint8ClampedArray')
  ));

  assert(!types.isUint16Array({ [Symbol.toStringTag]: 'Uint16Array' }));
  assert(types.isUint16Array(vm.runInNewContext('new Uint16Array')));

  assert(!types.isUint32Array({ [Symbol.toStringTag]: 'Uint32Array' }));
  assert(types.isUint32Array(vm.runInNewContext('new Uint32Array')));

  assert(!types.isInt8Array({ [Symbol.toStringTag]: 'Int8Array' }));
  assert(types.isInt8Array(vm.runInNewContext('new Int8Array')));

  assert(!types.isInt16Array({ [Symbol.toStringTag]: 'Int16Array' }));
  assert(types.isInt16Array(vm.runInNewContext('new Int16Array')));

  assert(!types.isInt32Array({ [Symbol.toStringTag]: 'Int32Array' }));
  assert(types.isInt32Array(vm.runInNewContext('new Int32Array')));

  assert(!types.isFloat16Array({ [Symbol.toStringTag]: 'Float16Array' }));
  assert(types.isFloat16Array(vm.runInNewContext('new Float16Array')));

  assert(!types.isFloat32Array({ [Symbol.toStringTag]: 'Float32Array' }));
  assert(types.isFloat32Array(vm.runInNewContext('new Float32Array')));

  assert(!types.isFloat64Array({ [Symbol.toStringTag]: 'Float64Array' }));
  assert(types.isFloat64Array(vm.runInNewContext('new Float64Array')));

  assert(!types.isBigInt64Array({ [Symbol.toStringTag]: 'BigInt64Array' }));
  assert(types.isBigInt64Array(vm.runInNewContext('new BigInt64Array')));

  assert(!types.isBigUint64Array({ [Symbol.toStringTag]: 'BigUint64Array' }));
  assert(types.isBigUint64Array(vm.runInNewContext('new BigUint64Array')));
}

{
  const primitive = true;
  const arrayBuffer = new ArrayBuffer();
  const buffer = Buffer.from(arrayBuffer);
  const dataView = new DataView(arrayBuffer);
  const uint8Array = new Uint8Array(arrayBuffer);
  const uint8ClampedArray = new Uint8ClampedArray(arrayBuffer);
  const uint16Array = new Uint16Array(arrayBuffer);
  const uint32Array = new Uint32Array(arrayBuffer);
  const int8Array = new Int8Array(arrayBuffer);
  const int16Array = new Int16Array(arrayBuffer);
  const int32Array = new Int32Array(arrayBuffer);
  const float16Array = new Float16Array(arrayBuffer);
  const float32Array = new Float32Array(arrayBuffer);
  const float64Array = new Float64Array(arrayBuffer);
  const bigInt64Array = new BigInt64Array(arrayBuffer);
  const bigUint64Array = new BigUint64Array(arrayBuffer);

  const fakeBuffer = { __proto__: Buffer.prototype };
  const fakeDataView = { __proto__: DataView.prototype };
  const fakeUint8Array = { __proto__: Uint8Array.prototype };
  const fakeUint8ClampedArray = { __proto__: Uint8ClampedArray.prototype };
  const fakeUint16Array = { __proto__: Uint16Array.prototype };
  const fakeUint32Array = { __proto__: Uint32Array.prototype };
  const fakeInt8Array = { __proto__: Int8Array.prototype };
  const fakeInt16Array = { __proto__: Int16Array.prototype };
  const fakeInt32Array = { __proto__: Int32Array.prototype };
  const fakeFloat16Array = { __proto__: Float16Array.prototype };
  const fakeFloat32Array = { __proto__: Float32Array.prototype };
  const fakeFloat64Array = { __proto__: Float64Array.prototype };
  const fakeBigInt64Array = { __proto__: BigInt64Array.prototype };
  const fakeBigUint64Array = { __proto__: BigUint64Array.prototype };

  const stealthyDataView =
    Object.setPrototypeOf(new DataView(arrayBuffer), Uint8Array.prototype);
  const stealthyUint8Array =
    Object.setPrototypeOf(new Uint8Array(arrayBuffer), ArrayBuffer.prototype);
  const stealthyUint8ClampedArray =
    Object.setPrototypeOf(
      new Uint8ClampedArray(arrayBuffer), ArrayBuffer.prototype
    );
  const stealthyUint16Array =
    Object.setPrototypeOf(new Uint16Array(arrayBuffer), Uint16Array.prototype);
  const stealthyUint32Array =
    Object.setPrototypeOf(new Uint32Array(arrayBuffer), Uint32Array.prototype);
  const stealthyInt8Array =
    Object.setPrototypeOf(new Int8Array(arrayBuffer), Int8Array.prototype);
  const stealthyInt16Array =
    Object.setPrototypeOf(new Int16Array(arrayBuffer), Int16Array.prototype);
  const stealthyInt32Array =
    Object.setPrototypeOf(new Int32Array(arrayBuffer), Int32Array.prototype);
  const stealthyFloat16Array =
    Object.setPrototypeOf(
      new Float16Array(arrayBuffer), Float16Array.prototype
    );
  const stealthyFloat32Array =
    Object.setPrototypeOf(
      new Float32Array(arrayBuffer), Float32Array.prototype
    );
  const stealthyFloat64Array =
    Object.setPrototypeOf(
      new Float64Array(arrayBuffer), Float64Array.prototype
    );
  const stealthyBigInt64Array =
    Object.setPrototypeOf(
      new BigInt64Array(arrayBuffer), BigInt64Array.prototype
    );
  const stealthyBigUint64Array =
    Object.setPrototypeOf(
      new BigUint64Array(arrayBuffer), BigUint64Array.prototype
    );

  const all = [
    primitive, arrayBuffer, buffer, fakeBuffer,
    dataView, fakeDataView, stealthyDataView,
    uint8Array, fakeUint8Array, stealthyUint8Array,
    uint8ClampedArray, fakeUint8ClampedArray, stealthyUint8ClampedArray,
    uint16Array, fakeUint16Array, stealthyUint16Array,
    uint32Array, fakeUint32Array, stealthyUint32Array,
    int8Array, fakeInt8Array, stealthyInt8Array,
    int16Array, fakeInt16Array, stealthyInt16Array,
    int32Array, fakeInt32Array, stealthyInt32Array,
    float16Array, fakeFloat16Array, stealthyFloat16Array,
    float32Array, fakeFloat32Array, stealthyFloat32Array,
    float64Array, fakeFloat64Array, stealthyFloat64Array,
    bigInt64Array, fakeBigInt64Array, stealthyBigInt64Array,
    bigUint64Array, fakeBigUint64Array, stealthyBigUint64Array,
  ];

  const expected = {
    isArrayBufferView: [
      buffer,
      dataView, stealthyDataView,
      uint8Array, stealthyUint8Array,
      uint8ClampedArray, stealthyUint8ClampedArray,
      uint16Array, stealthyUint16Array,
      uint32Array, stealthyUint32Array,
      int8Array, stealthyInt8Array,
      int16Array, stealthyInt16Array,
      int32Array, stealthyInt32Array,
      float16Array, stealthyFloat16Array,
      float32Array, stealthyFloat32Array,
      float64Array, stealthyFloat64Array,
      bigInt64Array, stealthyBigInt64Array,
      bigUint64Array, stealthyBigUint64Array,
    ],
    isTypedArray: [
      buffer,
      uint8Array, stealthyUint8Array,
      uint8ClampedArray, stealthyUint8ClampedArray,
      uint16Array, stealthyUint16Array,
      uint32Array, stealthyUint32Array,
      int8Array, stealthyInt8Array,
      int16Array, stealthyInt16Array,
      int32Array, stealthyInt32Array,
      float16Array, stealthyFloat16Array,
      float32Array, stealthyFloat32Array,
      float64Array, stealthyFloat64Array,
      bigInt64Array, stealthyBigInt64Array,
      bigUint64Array, stealthyBigUint64Array,
    ],
    isUint8Array: [
      buffer, uint8Array, stealthyUint8Array,
    ],
    isUint8ClampedArray: [
      uint8ClampedArray, stealthyUint8ClampedArray,
    ],
    isUint16Array: [
      uint16Array, stealthyUint16Array,
    ],
    isUint32Array: [
      uint32Array, stealthyUint32Array,
    ],
    isInt8Array: [
      int8Array, stealthyInt8Array,
    ],
    isInt16Array: [
      int16Array, stealthyInt16Array,
    ],
    isInt32Array: [
      int32Array, stealthyInt32Array,
    ],
    isFloat16Array: [
      float16Array, stealthyFloat16Array,
    ],
    isFloat32Array: [
      float32Array, stealthyFloat32Array,
    ],
    isFloat64Array: [
      float64Array, stealthyFloat64Array,
    ],
    isBigInt64Array: [
      bigInt64Array, stealthyBigInt64Array,
    ],
    isBigUint64Array: [
      bigUint64Array, stealthyBigUint64Array,
    ]
  };

  for (const testedFunc of Object.keys(expected)) {
    const func = types[testedFunc];
    const yup = [];
    for (const value of all) {
      if (func(value)) {
        yup.push(value);
      }
    }
    console.log('Testing', testedFunc);
    assert.deepStrictEqual(yup, expected[testedFunc]);
  }
}

(async () => {
  const m = new vm.SourceTextModule('');
  await m.link(() => 0);
  await m.evaluate();
  assert.ok(types.isModuleNamespaceObject(m.namespace));
})().then(common.mustCall());

{
  // eslint-disable-next-line node-core/crypto-check
  if (common.hasCrypto) {
    const crypto = require('crypto');
    assert.ok(!types.isKeyObject(crypto.createHash('sha1')));
  }
  assert.ok(!types.isCryptoKey());
  assert.ok(!types.isKeyObject());
}

// Fast path tests for the types module.

{
  function testIsDate(input) {
    return types.isDate(input);
  }

  eval('%PrepareFunctionForOptimization(testIsDate)');
  testIsDate(new Date());
  eval('%OptimizeFunctionOnNextCall(testIsDate)');
  assert.strictEqual(testIsDate(new Date()), true);
  assert.strictEqual(testIsDate(Math.random()), false);

  if (common.isDebug) {
    const { getV8FastApiCallCount } = internalBinding('debug');
    assert.strictEqual(getV8FastApiCallCount('types.isDate'), 2);
  }
}

{
  function testIsArgumentsObject(input) {
    return types.isArgumentsObject(input);
  }

  eval('%PrepareFunctionForOptimization(testIsArgumentsObject)');
  testIsArgumentsObject((function() { return arguments; })());
  eval('%OptimizeFunctionOnNextCall(testIsArgumentsObject)');
  assert.strictEqual(testIsArgumentsObject((function() { return arguments; })()), true);
  assert.strictEqual(testIsArgumentsObject(Math.random()), false);

  if (common.isDebug) {
    const { getV8FastApiCallCount } = internalBinding('debug');
    assert.strictEqual(getV8FastApiCallCount('types.isArgumentsObject'), 2);
  }
}

{
  function testIsBigIntObject(input) {
    return types.isBigIntObject(input);
  }

  eval('%PrepareFunctionForOptimization(testIsBigIntObject)');
  testIsBigIntObject(Object(BigInt(0)));
  eval('%OptimizeFunctionOnNextCall(testIsBigIntObject)');
  assert.strictEqual(testIsBigIntObject(Object(BigInt(0))), true);
  assert.strictEqual(testIsBigIntObject(Math.random()), false);

  if (common.isDebug) {
    const { getV8FastApiCallCount } = internalBinding('debug');
    assert.strictEqual(getV8FastApiCallCount('types.isBigIntObject'), 2);
  }
}

{
  function testIsBooleanObject(input) {
    return types.isBooleanObject(input);
  }

  eval('%PrepareFunctionForOptimization(testIsBooleanObject)');
  testIsBooleanObject(new Boolean());
  eval('%OptimizeFunctionOnNextCall(testIsBooleanObject)');
  assert.strictEqual(testIsBooleanObject(new Boolean()), true);
  assert.strictEqual(testIsBooleanObject(Math.random()), false);

  if (common.isDebug) {
    const { getV8FastApiCallCount } = internalBinding('debug');
    assert.strictEqual(getV8FastApiCallCount('types.isBooleanObject'), 2);
  }
}

{
  function testIsNumberObject(input) {
    return types.isNumberObject(input);
  }

  eval('%PrepareFunctionForOptimization(testIsNumberObject)');
  testIsNumberObject(new Number());
  eval('%OptimizeFunctionOnNextCall(testIsNumberObject)');
  assert.strictEqual(testIsNumberObject(new Number()), true);
  assert.strictEqual(testIsNumberObject(Math.random()), false);

  if (common.isDebug) {
    const { getV8FastApiCallCount } = internalBinding('debug');
    assert.strictEqual(getV8FastApiCallCount('types.isNumberObject'), 2);
  }
}

{
  function testIsStringObject(input) {
    return types.isStringObject(input);
  }

  eval('%PrepareFunctionForOptimization(testIsStringObject)');
  testIsStringObject(new String());
  eval('%OptimizeFunctionOnNextCall(testIsStringObject)');
  assert.strictEqual(testIsStringObject(new String()), true);
  assert.strictEqual(testIsStringObject(Math.random()), false);

  if (common.isDebug) {
    const { getV8FastApiCallCount } = internalBinding('debug');
    assert.strictEqual(getV8FastApiCallCount('types.isStringObject'), 2);
  }
}

{
  function testIsSymbolObject(input) {
    return types.isSymbolObject(input);
  }

  eval('%PrepareFunctionForOptimization(testIsSymbolObject)');
  testIsSymbolObject(Object(Symbol()));
  eval('%OptimizeFunctionOnNextCall(testIsSymbolObject)');
  assert.strictEqual(testIsSymbolObject(Object(Symbol())), true);
  assert.strictEqual(testIsSymbolObject(Math.random()), false);

  if (common.isDebug) {
    const { getV8FastApiCallCount } = internalBinding('debug');
    assert.strictEqual(getV8FastApiCallCount('types.isSymbolObject'), 2);
  }
}

{
  function testIsNativeError(input) {
    return types.isNativeError(input);
  }

  eval('%PrepareFunctionForOptimization(testIsNativeError)');
  testIsNativeError(new Error());
  eval('%OptimizeFunctionOnNextCall(testIsNativeError)');
  assert.strictEqual(testIsNativeError(new Error()), true);
  assert.strictEqual(testIsNativeError(Math.random()), false);

  if (common.isDebug) {
    const { getV8FastApiCallCount } = internalBinding('debug');
    assert.strictEqual(getV8FastApiCallCount('types.isNativeError'), 2);
  }
}

{
  function testIsRegExp(input) {
    return types.isRegExp(input);
  }

  eval('%PrepareFunctionForOptimization(testIsRegExp)');
  testIsRegExp(new RegExp());
  eval('%OptimizeFunctionOnNextCall(testIsRegExp)');
  assert.strictEqual(testIsRegExp(new RegExp()), true);
  assert.strictEqual(testIsRegExp(Math.random()), false);

  if (common.isDebug) {
    const { getV8FastApiCallCount } = internalBinding('debug');
    assert.strictEqual(getV8FastApiCallCount('types.isRegExp'), 2);
  }
}

{
  function testIsAsyncFunction(input) {
    return types.isAsyncFunction(input);
  }

  eval('%PrepareFunctionForOptimization(testIsAsyncFunction)');
  testIsAsyncFunction(async function() {});
  eval('%OptimizeFunctionOnNextCall(testIsAsyncFunction)');
  assert.strictEqual(testIsAsyncFunction(async function() {}), true);
  assert.strictEqual(testIsAsyncFunction(Math.random()), false);

  if (common.isDebug) {
    const { getV8FastApiCallCount } = internalBinding('debug');
    assert.strictEqual(getV8FastApiCallCount('types.isAsyncFunction'), 2);
  }
}

{
  function testIsGeneratorFunction(input) {
    return types.isGeneratorFunction(input);
  }

  eval('%PrepareFunctionForOptimization(testIsGeneratorFunction)');
  testIsGeneratorFunction(function*() {});
  eval('%OptimizeFunctionOnNextCall(testIsGeneratorFunction)');
  assert.strictEqual(testIsGeneratorFunction(function*() {}), true);
  assert.strictEqual(testIsGeneratorFunction(Math.random()), false);

  if (common.isDebug) {
    const { getV8FastApiCallCount } = internalBinding('debug');
    assert.strictEqual(getV8FastApiCallCount('types.isGeneratorFunction'), 2);
  }
}

{
  function testIsGeneratorObject(input) {
    return types.isGeneratorObject(input);
  }

  eval('%PrepareFunctionForOptimization(testIsGeneratorObject)');
  testIsGeneratorObject((function*() {})());
  eval('%OptimizeFunctionOnNextCall(testIsGeneratorObject)');
  assert.strictEqual(testIsGeneratorObject((function*() {})()), true);
  assert.strictEqual(testIsGeneratorObject(Math.random()), false);

  if (common.isDebug) {
    const { getV8FastApiCallCount } = internalBinding('debug');
    assert.strictEqual(getV8FastApiCallCount('types.isGeneratorObject'), 2);
  }
}

{
  function testIsPromise(input) {
    return types.isPromise(input);
  }

  eval('%PrepareFunctionForOptimization(testIsPromise)');
  testIsPromise(Promise.resolve());
  eval('%OptimizeFunctionOnNextCall(testIsPromise)');
  assert.strictEqual(testIsPromise(Promise.resolve()), true);
  assert.strictEqual(testIsPromise(Math.random()), false);

  if (common.isDebug) {
    const { getV8FastApiCallCount } = internalBinding('debug');
    assert.strictEqual(getV8FastApiCallCount('types.isPromise'), 2);
  }
}

{
  function testIsMap(input) {
    return types.isMap(input);
  }

  eval('%PrepareFunctionForOptimization(testIsMap)');
  testIsMap(new Map());
  eval('%OptimizeFunctionOnNextCall(testIsMap)');
  assert.strictEqual(testIsMap(new Map()), true);
  assert.strictEqual(testIsMap(Math.random()), false);

  if (common.isDebug) {
    const { getV8FastApiCallCount } = internalBinding('debug');
    assert.strictEqual(getV8FastApiCallCount('types.isMap'), 2);
  }
}

{
  function testIsSet(input) {
    return types.isSet(input);
  }

  eval('%PrepareFunctionForOptimization(testIsSet)');
  testIsSet(new Set());
  eval('%OptimizeFunctionOnNextCall(testIsSet)');
  assert.strictEqual(testIsSet(new Set()), true);
  assert.strictEqual(testIsSet(Math.random()), false);

  if (common.isDebug) {
    const { getV8FastApiCallCount } = internalBinding('debug');
    assert.strictEqual(getV8FastApiCallCount('types.isSet'), 2);
  }
}

{
  function testIsMapIterator(input) {
    return types.isMapIterator(input);
  }

  eval('%PrepareFunctionForOptimization(testIsMapIterator)');
  testIsMapIterator((new Map())[Symbol.iterator]());
  eval('%OptimizeFunctionOnNextCall(testIsMapIterator)');
  assert.strictEqual(testIsMapIterator((new Map())[Symbol.iterator]()), true);
  assert.strictEqual(testIsMapIterator(Math.random()), false);

  if (common.isDebug) {
    const { getV8FastApiCallCount } = internalBinding('debug');
    assert.strictEqual(getV8FastApiCallCount('types.isMapIterator'), 2);
  }
}

{
  function testIsSetIterator(input) {
    return types.isSetIterator(input);
  }

  eval('%PrepareFunctionForOptimization(testIsSetIterator)');
  testIsSetIterator((new Set())[Symbol.iterator]());
  eval('%OptimizeFunctionOnNextCall(testIsSetIterator)');
  assert.strictEqual(testIsSetIterator((new Set())[Symbol.iterator]()), true);
  assert.strictEqual(testIsSetIterator(Math.random()), false);

  if (common.isDebug) {
    const { getV8FastApiCallCount } = internalBinding('debug');
    assert.strictEqual(getV8FastApiCallCount('types.isSetIterator'), 2);
  }
}

{
  function testIsWeakMap(input) {
    return types.isWeakMap(input);
  }

  eval('%PrepareFunctionForOptimization(testIsWeakMap)');
  testIsWeakMap(new WeakMap());
  eval('%OptimizeFunctionOnNextCall(testIsWeakMap)');
  assert.strictEqual(testIsWeakMap(new WeakMap()), true);
  assert.strictEqual(testIsWeakMap(Math.random()), false);

  if (common.isDebug) {
    const { getV8FastApiCallCount } = internalBinding('debug');
    assert.strictEqual(getV8FastApiCallCount('types.isWeakMap'), 2);
  }
}

{
  function testIsWeakSet(input) {
    return types.isWeakSet(input);
  }

  eval('%PrepareFunctionForOptimization(testIsWeakSet)');
  testIsWeakSet(new WeakSet());
  eval('%OptimizeFunctionOnNextCall(testIsWeakSet)');
  assert.strictEqual(testIsWeakSet(new WeakSet()), true);
  assert.strictEqual(testIsWeakSet(Math.random()), false);

  if (common.isDebug) {
    const { getV8FastApiCallCount } = internalBinding('debug');
    assert.strictEqual(getV8FastApiCallCount('types.isWeakSet'), 2);
  }
}

{
  function testIsArrayBuffer(input) {
    return types.isArrayBuffer(input);
  }

  eval('%PrepareFunctionForOptimization(testIsArrayBuffer)');
  testIsArrayBuffer(new ArrayBuffer());
  eval('%OptimizeFunctionOnNextCall(testIsArrayBuffer)');
  assert.strictEqual(testIsArrayBuffer(new ArrayBuffer()), true);
  assert.strictEqual(testIsArrayBuffer(Math.random()), false);

  if (common.isDebug) {
    const { getV8FastApiCallCount } = internalBinding('debug');
    assert.strictEqual(getV8FastApiCallCount('types.isArrayBuffer'), 2);
  }
}

{
  function testIsDataView(input) {
    return types.isDataView(input);
  }

  eval('%PrepareFunctionForOptimization(testIsDataView)');
  testIsDataView(new DataView(new ArrayBuffer()));
  eval('%OptimizeFunctionOnNextCall(testIsDataView)');
  assert.strictEqual(testIsDataView(new DataView(new ArrayBuffer())), true);
  assert.strictEqual(testIsDataView(Math.random()), false);

  if (common.isDebug) {
    const { getV8FastApiCallCount } = internalBinding('debug');
    assert.strictEqual(getV8FastApiCallCount('types.isDataView'), 2);
  }
}

{
  function testIsSharedArrayBuffer(input) {
    return types.isSharedArrayBuffer(input);
  }

  eval('%PrepareFunctionForOptimization(testIsSharedArrayBuffer)');
  testIsSharedArrayBuffer(new SharedArrayBuffer());
  eval('%OptimizeFunctionOnNextCall(testIsSharedArrayBuffer)');
  assert.strictEqual(testIsSharedArrayBuffer(new SharedArrayBuffer()), true);
  assert.strictEqual(testIsSharedArrayBuffer(Math.random()), false);

  if (common.isDebug) {
    const { getV8FastApiCallCount } = internalBinding('debug');
    assert.strictEqual(getV8FastApiCallCount('types.isSharedArrayBuffer'), 2);
  }
}

{
  function testIsProxy(input) {
    return types.isProxy(input);
  }

  eval('%PrepareFunctionForOptimization(testIsProxy)');
  testIsProxy(new Proxy({}, {}));
  eval('%OptimizeFunctionOnNextCall(testIsProxy)');
  assert.strictEqual(testIsProxy(new Proxy({}, {})), true);
  assert.strictEqual(testIsProxy(Math.random()), false);

  if (common.isDebug) {
    const { getV8FastApiCallCount } = internalBinding('debug');
    assert.strictEqual(getV8FastApiCallCount('types.isProxy'), 2);
  }
}

{
  function testIsExternal(input) {
    return types.isExternal(input);
  }

  eval('%PrepareFunctionForOptimization(testIsExternal)');
  testIsExternal(external);
  eval('%OptimizeFunctionOnNextCall(testIsExternal)');
  assert.strictEqual(testIsExternal(external), true);
  assert.strictEqual(testIsExternal(Math.random()), false);

  if (common.isDebug) {
    const { getV8FastApiCallCount } = internalBinding('debug');
    assert.strictEqual(getV8FastApiCallCount('types.isExternal'), 2);
  }
}

{
  function testIsAnyArrayBuffer(input) {
    return types.isAnyArrayBuffer(input);
  }

  eval('%PrepareFunctionForOptimization(testIsAnyArrayBuffer)');
  testIsAnyArrayBuffer(new ArrayBuffer());
  eval('%OptimizeFunctionOnNextCall(testIsAnyArrayBuffer)');
  assert.strictEqual(testIsAnyArrayBuffer(new ArrayBuffer()), true);
  assert.strictEqual(testIsAnyArrayBuffer(Math.random()), false);

  if (common.isDebug) {
    const { getV8FastApiCallCount } = internalBinding('debug');
    assert.strictEqual(getV8FastApiCallCount('types.isAnyArrayBuffer'), 2);
  }
}

{
  function testIsBoxedPrimitive(input) {
    return types.isBoxedPrimitive(input);
  }

  eval('%PrepareFunctionForOptimization(testIsBoxedPrimitive)');
  testIsBoxedPrimitive(new String());
  eval('%OptimizeFunctionOnNextCall(testIsBoxedPrimitive)');
  assert.strictEqual(testIsBoxedPrimitive(new String()), true);
  assert.strictEqual(testIsBoxedPrimitive(Math.random()), false);

  if (common.isDebug) {
    const { getV8FastApiCallCount } = internalBinding('debug');
    assert.strictEqual(getV8FastApiCallCount('types.isBoxedPrimitive'), 2);
  }
}
