'use strict';

const common = require('../common.js');
const assert = require('assert');

const bench = common.createBenchmark(main, {
  n: [10, 50, 200],
  size: [1e3],
  datasetName: [
    'objects',
    'sets',
    'maps',
    'circularRefs',
    'typedArrays',
    'arrayBuffers',
    'dataViewArrayBuffers',
  ],
});

function createObjects(length, depth = 0) {
  return Array.from({ length }, () => ({
    foo: 'yarp',
    nope: {
      bar: '123',
      a: [1, 2, 3],
      c: {},
      b: !depth ? createObjects(2, depth + 1) : [],
    },
  }));
}

function createSets(length, depth = 0) {
  return Array.from({ length }, () => new Set([
    'yarp',
    '123',
    1,
    2,
    3,
    null,
    {
      simple: 'object',
      number: 42,
    },
    ['array', 'with', 'values'],
    !depth ? new Set([1, 2, { nested: true }]) : new Set(),
    !depth ? createSets(2, depth + 1) : null,
  ]));
}

function createMaps(length, depth = 0) {
  return Array.from({ length }, () => new Map([
    ['primitiveKey', 'primitiveValue'],
    [42, 'numberKey'],
    ['objectValue', { a: 1, b: 2 }],
    ['arrayValue', [1, 2, 3]],
    ['nestedMap', new Map([['a', 1], ['b', { deep: true }]])],
    [{ objectKey: true }, 'value from object key'],
    [[1, 2, 3], 'value from array key'],
    [!depth ? createMaps(2, depth + 1) : null, 'recursive value'],
  ]));
}

function createCircularRefs(length) {
  return Array.from({ length }, () => {
    const circularSet = new Set();
    const circularMap = new Map();
    const circularObj = { name: 'circular object' };

    circularSet.add('some value');
    circularSet.add(circularSet);

    circularMap.set('self', circularMap);
    circularMap.set('value', 'regular value');

    circularObj.self = circularObj;

    const objA = { name: 'A' };
    const objB = { name: 'B' };
    objA.ref = objB;
    objB.ref = objA;

    circularSet.add(objA);
    circularMap.set('objB', objB);

    return {
      circularSet,
      circularMap,
      circularObj,
      objA,
      objB,
    };
  });
}

function createTypedArrays(length) {
  return Array.from({ length }, () => {
    const buffer = new ArrayBuffer(32);

    return {
      int8: new Int8Array(buffer, 0, 4),
      uint8: new Uint8Array(buffer, 4, 4),
      uint8Clamped: new Uint8ClampedArray(buffer, 8, 4),
      int16: new Int16Array([1, 2, 3]),
      uint16: new Uint16Array([1, 2, 3]),
      int32: new Int32Array([1, 2, 3]),
      uint32: new Uint32Array([1, 2, 3]),
      float32: new Float32Array([1.1, 2.2, 3.3]),
      float64: new Float64Array([1.1, 2.2, 3.3]),
      bigInt64: new BigInt64Array([1n, 2n, 3n]),
      bigUint64: new BigUint64Array([1n, 2n, 3n]),
    };
  });
}

function createArrayBuffers(length) {
  return Array.from({ length }, (_, n) => new ArrayBuffer(n));
}

function createDataViewArrayBuffers(length) {
  return Array.from({ length }, (_, n) => new DataView(new ArrayBuffer(n)));
}

const datasetMappings = {
  objects: createObjects,
  sets: createSets,
  maps: createMaps,
  circularRefs: createCircularRefs,
  typedArrays: createTypedArrays,
  arrayBuffers: createArrayBuffers,
  dataViewArrayBuffers: createDataViewArrayBuffers,
};

function getDatasets(datasetName, size) {
  return {
    actual: datasetMappings[datasetName](size),
    expected: datasetMappings[datasetName](size),
  };
}

function main({ size, n, datasetName }) {
  const { actual, expected } = getDatasets(datasetName, size);

  bench.start();
  for (let i = 0; i < n; ++i) {
    assert.partialDeepStrictEqual(actual, expected);
  }
  bench.end(n);
}
