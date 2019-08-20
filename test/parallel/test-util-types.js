// Flags: --experimental-vm-modules --expose-internals
'use strict';
require('../common');
const fixtures = require('../common/fixtures');
const assert = require('assert');
const { types, inspect } = require('util');
const vm = require('vm');
const { internalBinding } = require('internal/test/binding');
const { JSStream } = internalBinding('js_stream');

const external = (new JSStream())._externalStream;
const wasmBuffer = fixtures.readSync('simple.wasm');

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
  [ new WebAssembly.Module(wasmBuffer), 'isWebAssemblyCompiledModule' ],
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
  Object(BigInt(0))
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
  const float32Array = new Float32Array(arrayBuffer);
  const float64Array = new Float64Array(arrayBuffer);
  const bigInt64Array = new BigInt64Array(arrayBuffer);
  const bigUint64Array = new BigUint64Array(arrayBuffer);

  const fakeBuffer = Object.create(Buffer.prototype);
  const fakeDataView = Object.create(DataView.prototype);
  const fakeUint8Array = Object.create(Uint8Array.prototype);
  const fakeUint8ClampedArray = Object.create(Uint8ClampedArray.prototype);
  const fakeUint16Array = Object.create(Uint16Array.prototype);
  const fakeUint32Array = Object.create(Uint32Array.prototype);
  const fakeInt8Array = Object.create(Int8Array.prototype);
  const fakeInt16Array = Object.create(Int16Array.prototype);
  const fakeInt32Array = Object.create(Int32Array.prototype);
  const fakeFloat32Array = Object.create(Float32Array.prototype);
  const fakeFloat64Array = Object.create(Float64Array.prototype);
  const fakeBigInt64Array = Object.create(BigInt64Array.prototype);
  const fakeBigUint64Array = Object.create(BigUint64Array.prototype);

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
    float32Array, fakeFloat32Array, stealthyFloat32Array,
    float64Array, fakeFloat64Array, stealthyFloat64Array,
    bigInt64Array, fakeBigInt64Array, stealthyBigInt64Array,
    bigUint64Array, fakeBigUint64Array, stealthyBigUint64Array
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
      float32Array, stealthyFloat32Array,
      float64Array, stealthyFloat64Array,
      bigInt64Array, stealthyBigInt64Array,
      bigUint64Array, stealthyBigUint64Array
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
      float32Array, stealthyFloat32Array,
      float64Array, stealthyFloat64Array,
      bigInt64Array, stealthyBigInt64Array,
      bigUint64Array, stealthyBigUint64Array
    ],
    isUint8Array: [
      buffer, uint8Array, stealthyUint8Array
    ],
    isUint8ClampedArray: [
      uint8ClampedArray, stealthyUint8ClampedArray
    ],
    isUint16Array: [
      uint16Array, stealthyUint16Array
    ],
    isUint32Array: [
      uint32Array, stealthyUint32Array
    ],
    isInt8Array: [
      int8Array, stealthyInt8Array
    ],
    isInt16Array: [
      int16Array, stealthyInt16Array
    ],
    isInt32Array: [
      int32Array, stealthyInt32Array
    ],
    isFloat32Array: [
      float32Array, stealthyFloat32Array
    ],
    isFloat64Array: [
      float64Array, stealthyFloat64Array
    ],
    isBigInt64Array: [
      bigInt64Array, stealthyBigInt64Array
    ],
    isBigUint64Array: [
      bigUint64Array, stealthyBigUint64Array
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
  m.instantiate();
  await m.evaluate();
  assert.ok(types.isModuleNamespaceObject(m.namespace));
})();
