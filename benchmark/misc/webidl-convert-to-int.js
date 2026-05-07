'use strict';

const assert = require('assert');
const common = require('../common.js');

const bench = common.createBenchmark(main, {
  converter: [
    'byte',
    'octet',
    'unsigned short',
    'unsigned long',
    'long long',
  ],
  input: [
    'integer',
    'fractional',
    'wrap',
    'clamp',
    'enforce-range',
    'object',
  ],
  n: [1e6],
}, { flags: ['--expose-internals'] });

function getConverter(converter) {
  switch (converter) {
    case 'byte':
      return { bitLength: 8, signedness: 'signed' };
    case 'octet':
      return { bitLength: 8 };
    case 'unsigned short':
      return { bitLength: 16 };
    case 'unsigned long':
      return { bitLength: 32 };
    case 'long long':
      return { bitLength: 64, signedness: 'signed' };
    default:
      throw new Error(`Unsupported converter: ${converter}`);
  }
}

function getInput(input) {
  switch (input) {
    case 'integer':
      return { value: 7 };
    case 'fractional':
      return { value: 7.9 };
    case 'wrap':
      return { value: 2 ** 63 + 2 ** 11 };
    case 'clamp':
      return { value: 300.8, options: { clamp: true } };
    case 'enforce-range':
      return { value: 7.9, options: { enforceRange: true } };
    case 'object':
      return {
        value: {
          valueOf() { return 7; },
        },
      };
    default:
      throw new Error(`Unsupported input: ${input}`);
  }
}

function main({ n, converter, input }) {
  const { convertToInt } = require('internal/webidl');
  const { bitLength, signedness } = getConverter(converter);
  const { value, options } = getInput(input);

  let noDead;
  bench.start();
  if (options === undefined) {
    for (let i = 0; i < n; i++)
      noDead = convertToInt(value, bitLength, signedness);
  } else {
    for (let i = 0; i < n; i++)
      noDead = convertToInt(value, bitLength, signedness, options);
  }
  bench.end(n);

  assert.strictEqual(typeof noDead, 'number');
}
