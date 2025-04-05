'use strict';

const common = require('../common.js');
const { deepEqual, deepStrictEqual, notDeepEqual, notDeepStrictEqual } =
  require('assert');

const bench = common.createBenchmark(main, {
  n: [1e3],
  len: [2, 1e2],
  strict: [0, 1],
  order: ['insert', 'random', 'reversed'],
  method: [
    'deepEqual_primitiveOnly',
    'deepEqual_objectOnly',
    'deepEqual_mixed',
    'notDeepEqual_primitiveOnly',
    'notDeepEqual_objectOnly',
    'notDeepEqual_mixed',
  ],
}, {
  combinationFilter(p) {
    return p.order !== 'random' || p.strict === 1 && p.method !== 'notDeepEqual_objectOnly';
  },
});

function shuffleArray(array) {
  for (let i = array.length - 1; i > 0; i--) {
    const j = Math.floor(Math.random() * (i + 1));
    const temp = array[i];
    array[i] = array[j];
    array[j] = temp;
  }
}

function benchmark(method, n, values, values2, order) {
  const actual = new Set(values);
  // Prevent reference equal elements
  let deepCopy = JSON.parse(JSON.stringify(values2));
  if (order === 'reversed') {
    deepCopy = deepCopy.reverse();
  } else if (order === 'random') {
    shuffleArray(deepCopy);
  }
  const expected = new Set(deepCopy);
  bench.start();
  for (let i = 0; i < n; ++i) {
    method(actual, expected);
  }
  bench.end(n);
}

function main({ n, len, method, strict, order }) {
  const array = Array.from({ length: len }, () => '');

  switch (method) {
    case 'deepEqual_primitiveOnly': {
      const values = array.map((_, i) => `str_${i}`);
      benchmark(strict ? deepStrictEqual : deepEqual, n, values, values, order);
      break;
    }
    case 'deepEqual_objectOnly': {
      const values = array.map((_, i) => [`str_${i}`, null]);
      benchmark(strict ? deepStrictEqual : deepEqual, n, values, values, order);
      break;
    }
    case 'deepEqual_mixed': {
      const values = array.map((_, i) => {
        return i % 2 ? [`str_${i}`, null] : `str_${i}`;
      });
      benchmark(strict ? deepStrictEqual : deepEqual, n, values, values, order);
      break;
    }
    case 'notDeepEqual_primitiveOnly': {
      const values = array.map((_, i) => `str_${i}`);
      const values2 = values.slice(0);
      values2[Math.floor(len / 2)] = 'w00t';
      benchmark(strict ? notDeepStrictEqual : notDeepEqual, n, values, values2, order);
      break;
    }
    case 'notDeepEqual_objectOnly': {
      const values = array.map((_, i) => [`str_${i}`, null]);
      const values2 = values.slice(0);
      values2[Math.floor(len / 2)] = ['w00t'];
      benchmark(strict ? notDeepStrictEqual : notDeepEqual, n, values, values2, order);
      break;
    }
    case 'notDeepEqual_mixed': {
      const values = array.map((_, i) => {
        return i % 2 ? [`str_${i}`, null] : `str_${i}`;
      });
      const values2 = values.slice();
      values2[0] = 'w00t';
      benchmark(strict ? notDeepStrictEqual : notDeepEqual, n, values, values2, order);
      break;
    }
    default:
      throw new Error(`Unsupported method "${method}"`);
  }
}
