'use strict';
const common = require('../common.js');
const assert = require('assert');

const circular = {};
circular.circular = circular;
const circular2 = {};
circular2.circular = circular2;
const notCircular = {};
notCircular.circular = {};

const primValues = {
  'string': 'abcdef',
  'number': 1_000,
  'boolean': true,
  'object': { property: 'abcdef' },
  'array': [1, 2, 3],
  'set_object': new Set([[1]]),
  'set_simple': new Set([1, 2, 3]),
  'circular': circular,
  'empty_object': {},
  'regexp': /abc/i,
  'date': new Date(),
};

const primValues2 = {
  'object': { property: 'abcdef' },
  'array': [1, 2, 3],
  'set_object': new Set([[1]]),
  'set_simple': new Set([1, 3, 2]),
  'circular': circular2,
  'empty_object': {},
  'regexp': /abc/i,
  'date': new Date(primValues.date),
};

const primValuesUnequal = {
  'string': 'abcdez',
  'number': 1_001,
  'boolean': false,
  'object': { property2: 'abcdef' },
  'array': [1, 3, 2],
  'set_object': new Set([[2]]),
  'set_simple': new Set([1, 4, 2]),
  'circular': notCircular,
  'empty_object': [],
  'regexp': /abc/g,
  'date': new Date(primValues.date.getTime() + 1),
};

const bench = common.createBenchmark(main, {
  primitive: Object.keys(primValues),
  n: [1e5],
  strict: [0, 1],
  method: ['deepEqual', 'notDeepEqual'],
}, {
  combinationFilter: (p) => {
    return p.strict === 1 || p.method === 'deepEqual';
  },
});

function main({ n, primitive, method, strict }) {
  const prim = primValues[primitive];
  const actual = primValues2[primitive] ?? prim;
  const expected = method.includes('not') ? primValuesUnequal[primitive] : prim;

  if (strict) {
    method = method.replace('eep', 'eepStrict');
  }
  const fn = assert[method];

  bench.start();
  for (let i = 0; i < n; ++i) {
    fn(actual, expected);
  }
  bench.end(n);
}
