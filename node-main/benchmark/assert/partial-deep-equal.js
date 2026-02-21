'use strict';

const common = require('../common.js');
const assert = require('assert');

const bench = common.createBenchmark(main, {
  n: [125],
  size: [500],
  extraProps: [0, 1],
  datasetName: [
    'objects',
    'sets',
    'setsWithObjects',
    'maps',
    'circularRefs',
    'typedArrays',
    'arrayBuffers',
    'dataViewArrayBuffers',
    'array',
  ],
});

function createArray(length, extraProps) {
  if (extraProps) {
    return Array.from({ length: length * 4 }, (_, i) => i);
  }
  return Array.from({ length }, (_, i) => i * 4);
}

function createObjects(length, extraProps, depth = 0) {
  return Array.from({ length }, (_, i) => ({
    foo: 'yarp',
    nope: {
      bar: '123',
      ...(extraProps ? { a: [1, 2, i] } : {}),
      c: {},
      b: !depth ? createObjects(2, extraProps, depth + 1) : [],
    },
  }));
}

function createSetsWithObjects(length, extraProps, depth = 0) {
  return Array.from({ length }, (_, i) => new Set([
    ...(extraProps ? [{}] : []),
    {
      simple: 'object',
      number: i,
    },
    ['array', 'with', 'values'],
    new Set([[], {}, { nested: i }]),
  ]));
}

function createSets(length, extraProps, depth = 0) {
  return Array.from({ length }, (_, i) => new Set([
    'yarp',
    ...(extraProps ? ['123', 1, 2] : []),
    i + 3,
    null,
    {
      simple: 'object',
      number: i,
    },
    ['array', 'with', 'values'],
    !depth ? new Set([1, { nested: i }]) : new Set(),
    !depth ? createSets(2, extraProps, depth + 1) : null,
  ]));
}

function createMaps(length, extraProps, depth = 0) {
  return Array.from({ length }, (_, i) => new Map([
    ...(extraProps ? [['primitiveKey', 'primitiveValue']] : []),
    [42, 'numberKey'],
    ['objectValue', { a: 1, b: i }],
    ['arrayValue', [1, 2, i]],
    ['nestedMap', new Map([['a', i], ['b', { deep: true }]])],
    [{ objectKey: true }, 'value from object key'],
    [[1, i, 3], 'value from array key'],
    [!depth ? createMaps(2, extraProps, depth + 1) : null, 'recursive value' + i],
  ]));
}

function createCircularRefs(length, extraProps) {
  return Array.from({ length }, (_, i) => {
    const circularSet = new Set();
    const circularMap = new Map();
    const circularObj = { name: 'circular object' };

    circularSet.add('some value' + i);
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
      ...extraProps ? { extra: i } : {},
      circularObj,
      objA,
      objB,
    };
  });
}

function createTypedArrays(length, extraParts) {
  const extra = extraParts ? [9, 8, 7] : [];
  return Array.from({ length }, (_, i) => {
    return {
      uint8: new Uint8Array(new ArrayBuffer(32), 4, 4),
      int16: new Int16Array([1, 2, ...extra, 3]),
      uint32: new Uint32Array([i + 1, i + 2, ...extra, i + 3]),
      float64: new Float64Array([1.1, 2.2, ...extra, i + 3.3]),
      bigUint64: new BigUint64Array([1n, 2n, 3n]),
    };
  });
}

function createArrayBuffers(length, extra) {
  return Array.from({ length }, (_, n) => {
    const buffer = Buffer.alloc(n + (extra ? 1 : 0));
    for (let i = 0; i < n; i++) {
      buffer.writeInt8(i % 128, i);
    }
    return buffer.buffer;
  });
}

function createDataViewArrayBuffers(length, extra) {
  return createArrayBuffers(length, extra).map((buffer) => new DataView(buffer));
}

const datasetMappings = {
  objects: createObjects,
  sets: createSets,
  setsWithObjects: createSetsWithObjects,
  maps: createMaps,
  circularRefs: createCircularRefs,
  typedArrays: createTypedArrays,
  arrayBuffers: createArrayBuffers,
  dataViewArrayBuffers: createDataViewArrayBuffers,
  array: createArray,
};

function getDatasets(datasetName, size, extra) {
  return {
    actual: datasetMappings[datasetName](size, true),
    expected: datasetMappings[datasetName](size, !extra),
  };
}

function main({ size, n, datasetName, extraProps }) {
  const { actual, expected } = getDatasets(datasetName, size, extraProps);

  bench.start();
  for (let i = 0; i < n; ++i) {
    assert.partialDeepStrictEqual(actual, expected);
  }
  bench.end(n);
}
