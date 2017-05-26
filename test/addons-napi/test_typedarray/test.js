'use strict';
const common = require('../../common');
const assert = require('assert');

// Testing api calls for arrays
const test_typedarray = require(`./build/${common.buildType}/test_typedarray`);

const byteArray = new Uint8Array(3);
byteArray[0] = 0;
byteArray[1] = 1;
byteArray[2] = 2;
assert.strictEqual(byteArray.length, 3);

const doubleArray = new Float64Array(3);
doubleArray[0] = 0.0;
doubleArray[1] = 1.1;
doubleArray[2] = 2.2;
assert.strictEqual(doubleArray.length, 3);

const byteResult = test_typedarray.Multiply(byteArray, 3);
assert.ok(byteResult instanceof Uint8Array);
assert.strictEqual(byteResult.length, 3);
assert.strictEqual(byteResult[0], 0);
assert.strictEqual(byteResult[1], 3);
assert.strictEqual(byteResult[2], 6);

const doubleResult = test_typedarray.Multiply(doubleArray, -3);
assert.ok(doubleResult instanceof Float64Array);
assert.strictEqual(doubleResult.length, 3);
assert.strictEqual(doubleResult[0], 0);
assert.strictEqual(Math.round(10 * doubleResult[1]) / 10, -3.3);
assert.strictEqual(Math.round(10 * doubleResult[2]) / 10, -6.6);

const externalResult = test_typedarray.External();
assert.ok(externalResult instanceof Int8Array);
assert.strictEqual(externalResult.length, 3);
assert.strictEqual(externalResult[0], 0);
assert.strictEqual(externalResult[1], 1);
assert.strictEqual(externalResult[2], 2);

// validate creation of all kinds of TypedArrays
const buffer = new ArrayBuffer(128);
const arrayTypes = [ Int8Array, Uint8Array, Uint8ClampedArray, Int16Array,
                     Uint16Array, Int32Array, Uint32Array, Float32Array,
                     Float64Array ];

arrayTypes.forEach((currentType, key) => {
  const template = Reflect.construct(currentType, buffer);
  const theArray = test_typedarray.CreateTypedArray(template, buffer);

  assert.ok(theArray instanceof currentType,
            'Type of new array should match that of the template');
  assert.notStrictEqual(theArray,
                        template,
                        'the new array should not be a copy of the template');
  assert.strictEqual(theArray.buffer,
                     buffer,
                     'Buffer for array should match the one passed in');
});
