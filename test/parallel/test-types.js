'use strict';

// Flags: --expose-internals

require('../common');
const assert = require('assert');
const types = require('internal/util/types');

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
