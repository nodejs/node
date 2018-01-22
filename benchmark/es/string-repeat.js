'use strict';

const assert = require('assert');
const common = require('../common.js');

const configs = {
  n: [1e3],
  mode: ['Array', 'repeat'],
  encoding: ['ascii', 'utf8'],
  size: [1e1, 1e3, 1e6],
};

const bench = common.createBenchmark(main, configs);

function main({ n, size, encoding, mode }) {
  const character = encoding === 'ascii' ? 'a' : '\ud83d\udc0e'; // '🐎'

  let str;

  switch (mode) {
    case '':
      // Empty string falls through to next line as default, mostly for tests.
    case 'Array':
      bench.start();
      for (let i = 0; i < n; i++)
        str = new Array(size + 1).join(character);
      bench.end(n);
      break;
    case 'repeat':
      bench.start();
      for (let i = 0; i < n; i++)
        str = character.repeat(size);
      bench.end(n);
      break;
    default:
      throw new Error('Unexpected method');
  }

  assert.strictEqual([...str].length, size);
}
