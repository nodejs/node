// Flags: --harmony-bigint
/* global SharedArrayBuffer */
'use strict';
const common = require('../common');
const fixtures = require('../common/fixtures');
const assert = require('assert');
const { types, inspect } = require('util');
const path = require('path');
const fs = require('fs');
const vm = require('vm');
const { JSStream } = process.binding('js_stream');

common.crashOnUnhandledRejection();

const external = (new JSStream())._externalStream;
const wasmBuffer = fixtures.readSync('test.wasm');

for (const [ value, _method ] of [
  [ external, 'isExternal' ],
  [ new Date() ],
  [ (function() { return arguments; })(), 'isArgumentsObject' ],
  [ new Boolean(), 'isBooleanObject' ],
  [ new Number(), 'isNumberObject' ],
  [ new String(), 'isStringObject' ],
  [ Object(Symbol()), 'isSymbolObject' ],
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
         types.isAnyArrayBuffer(value)) && key.includes('Array')) {
      continue;
    }

    assert.strictEqual(types[key](value),
                       key === method,
                       `${inspect(value)}: ${key}, ` +
                       `${method}, ${types[key](value)}`);
  }
}

{
  assert(!types.isUint8Array({ [Symbol.toStringTag]: 'Uint8Array' }));
  assert(types.isUint8Array(vm.runInNewContext('new Uint8Array')));
}

{
  const primitive = true;
  const arrayBuffer = new ArrayBuffer();
  const dataView = new DataView(arrayBuffer);
  const int32Array = new Int32Array(arrayBuffer);
  const uint8Array = new Uint8Array(arrayBuffer);
  const buffer = Buffer.from(arrayBuffer);

  const fakeDataView = Object.create(DataView.prototype);
  const fakeInt32Array = Object.create(Int32Array.prototype);
  const fakeUint8Array = Object.create(Uint8Array.prototype);
  const fakeBuffer = Object.create(Buffer.prototype);

  const stealthyDataView =
      Object.setPrototypeOf(new DataView(arrayBuffer), Uint8Array.prototype);
  const stealthyInt32Array =
      Object.setPrototypeOf(new Int32Array(arrayBuffer), uint8Array);
  const stealthyUint8Array =
      Object.setPrototypeOf(new Uint8Array(arrayBuffer), ArrayBuffer.prototype);

  const all = [
    primitive, arrayBuffer, dataView, int32Array, uint8Array, buffer,
    fakeDataView, fakeInt32Array, fakeUint8Array, fakeBuffer,
    stealthyDataView, stealthyInt32Array, stealthyUint8Array
  ];

  const expected = {
    isArrayBufferView: [
      dataView, int32Array, uint8Array, buffer,
      stealthyDataView, stealthyInt32Array, stealthyUint8Array
    ],
    isTypedArray: [
      int32Array, uint8Array, buffer, stealthyInt32Array, stealthyUint8Array
    ],
    isUint8Array: [
      uint8Array, buffer, stealthyUint8Array
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


// Try reading the v8.h header to verify completeness.

let v8_h;
try {
  v8_h = fs.readFileSync(path.resolve(
    __dirname, '..', '..', 'deps', 'v8', 'include', 'v8.h'), 'utf8');
} catch (e) {
  // If loading the header fails, it should fail because we did not find it.
  assert.strictEqual(e.code, 'ENOENT');
  common.skip('Could not read v8.h');
  return;
}

// Exclude a number of checks that make sense on the C++ side but have
// much faster/better JS equivalents, so they should not be exposed.
const exclude = [
  'Undefined', 'Null', 'NullOrUndefined', 'True', 'False', 'Name', 'String',
  'Symbol', 'Function', 'Array', 'Object', 'Boolean', 'Number', 'Int32',
  'Uint32'
];

const start = v8_h.indexOf('Value : public Data');
const end = v8_h.indexOf('};', start);
const valueDefinition = v8_h.substr(start, end - start);

const re = /bool Is(\w+)\(\)/g;
let match;
while (match = re.exec(valueDefinition)) {
  if (exclude.includes(match[1]))
    continue;
  assert(`is${match[1]}` in types,
         `util.types should provide check for Is${match[1]}`);
}
