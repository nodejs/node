'use strict';

const util = require('node:util');
const assert = require('node:assert');
const common = require('../common');

const inputs = {
  'short-flags': {
    args: ['-a', '-b', '-c'],
    options: {
      alpha: { type: 'boolean', short: 'a' },
      beta: { type: 'boolean', short: 'b' },
      gamma: { type: 'boolean', short: 'c' },
    },
  },
  'long-flags': {
    args: ['--foo', '--bar', '--baz'],
    options: {
      foo: { type: 'boolean' },
      bar: { type: 'boolean' },
      baz: { type: 'boolean' },
    },
  },
  'values': {
    args: ['--foo=1', '--bar', '2', '-b', '3'],
    options: {
      foo: { type: 'string' },
      bar: { type: 'string' },
      baz: { type: 'string', short: 'b' },
    },
  },
  'non-strict': {
    args: ['--foo', '--unknown=value', '-x', 'file.js', 'other.js'],
    options: {
      foo: { type: 'boolean' },
    },
    strict: false,
  },
  'tokens': {
    args: ['-ab', '--foo=1', '--bar', '2', 'file.js'],
    options: {
      alpha: { type: 'boolean', short: 'a' },
      beta: { type: 'boolean', short: 'b' },
      foo: { type: 'string' },
      bar: { type: 'string' },
    },
    allowPositionals: true,
    tokens: true,
  },
};

const bench = common.createBenchmark(main, {
  n: [1e5],
  type: Object.keys(inputs),
});

function main({ n, type }) {
  const config = inputs[type];
  let noDead;

  bench.start();
  for (let i = 0; i < n; i++) {
    noDead = util.parseArgs(config);
  }
  bench.end(n);
  assert(noDead !== undefined);
}
