'use strict';

require('../common');
const assert = require('assert');
const a = require('assert');

function makeBlock(f) {
  var args = Array.prototype.slice.call(arguments, 1);
  return function() {
    return f.apply(this, args);
  };
}

const equalArrayPairs = [
  [new Uint8Array(1e5), new Uint8Array(1e5)],
  [new Uint16Array(1e5), new Uint16Array(1e5)],
  [new Uint32Array(1e5), new Uint32Array(1e5)],
  [new Uint8ClampedArray(1e5), new Uint8ClampedArray(1e5)],
  [new Int8Array(1e5), new Int8Array(1e5)],
  [new Int16Array(1e5), new Int16Array(1e5)],
  [new Int32Array(1e5), new Int32Array(1e5)],
  [new Float32Array(1e5), new Float32Array(1e5)],
  [new Float64Array(1e5), new Float64Array(1e5)]
];

const notEqualArrayPairs = [
  [new Uint8Array(2), new Uint8Array(3)],
  [new Uint8Array([1, 2, 3]), new Uint8Array([4, 5, 6])],
  [new Uint8ClampedArray([300, 2, 3]), new Uint8Array([300, 2, 3])]
];

equalArrayPairs.forEach((arrayPair) => {
  assert.deepEqual(arrayPair[0], arrayPair[1]);
});

notEqualArrayPairs.forEach((arrayPair) => {
  assert.throws(
    makeBlock(a.deepEqual, arrayPair[0], arrayPair[1]),
    a.AssertionError
  );
});
