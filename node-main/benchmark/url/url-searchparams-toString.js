'use strict';
const common = require('../common.js');

const values = {
  noencode: {
    foo: 'bar',
    baz: 'quux',
    xyzzy: 'thud',
  },
  encodemany: {
    '\u0080\u0083\u0089': 'bar',
    '\u008C\u008E\u0099': 'quux',
    'xyzzy': '\u00A5q\u00A3r',
  },
  encodelast: {
    foo: 'bar',
    baz: 'quux',
    xyzzy: 'thu\u00AC',
  },
  array: {
    foo: [],
    baz: ['bar'],
    xyzzy: ['bar', 'quux', 'thud'],
  },
  multiprimitives: {
    foo: false,
    bar: -13.37,
    baz: 'baz',
  },
};

function paramGenerator(paramType) {
  const valueKeys = Object.keys(values);
  switch (paramType) {
    case 'string':
      // Return the values object with all values as strings
      return valueKeys.reduce((acc, key) => {
        const objectKeys = Object.keys(values[key]);
        acc[key] = objectKeys.reduce((acc, k, i) => {
          acc += `${k}=${values[key][k]}${i < objectKeys.length - 1 ? '&' : ''}`;
          return acc;
        }, '');
        return acc;
      }, {});
    case 'iterable':
      // Return the values object with all values as iterable
      return valueKeys.reduce((acc, key) => {
        acc[key] = Object.keys(values[key]).reduce((acc, k) => {
          acc.push([k, values[key][k]]);
          return acc;
        }, []);
        return acc;
      }, {});
    case 'object':
      // Return the values object with all values as objects
      return values;
    default:
  }
}

const bench = common.createBenchmark(main, {
  type: ['noencode', 'encodemany', 'encodelast', 'array', 'multiprimitives'],
  inputType: ['string', 'iterable', 'object'],
  n: [1e6],
});

function main({ n, type, inputType }) {
  const inputs = paramGenerator(inputType);
  const input = inputs[type];
  const u = new URLSearchParams(input);

  bench.start();
  for (let i = 0; i < n; i += 1)
    u.toString();
  bench.end(n);
}
