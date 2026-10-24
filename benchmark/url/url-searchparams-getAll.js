'use strict';

const common = require('../common');
const assert = require('node:assert');

const bench = common.createBenchmark(main, {
  n: [1e6],
  size: [4, 32, 256],
  matches: ['none', 'one', 'half', 'all'],
  mode: ['read', 'construct'],
});

function main({ n, size, matches, mode }) {
  const params = new URLSearchParams();
  const expected = [];
  for (let index = 0; index < size; index++) {
    const match = matches === 'all' ||
      (matches === 'half' && index % 2 === 0) ||
      (matches === 'one' && index === size - 1);
    const value = `${index}`;
    params.append(match ? 'target' : `key${index}`, value);
    if (match) expected.push(value);
  }

  const query = params.toString();
  let checksum = 0;
  let result;
  bench.start();
  if (mode === 'read') {
    for (let iteration = 0; iteration < n; iteration++) {
      result = params.getAll('target');
      checksum += result.length;
      if (result.length !== 0) checksum += result[result.length - 1].length;
    }
  } else {
    for (let iteration = 0; iteration < n; iteration++) {
      result = new URLSearchParams(query).getAll('target');
      checksum += result.length;
      if (result.length !== 0) checksum += result[result.length - 1].length;
    }
  }
  bench.end(n);
  assert.deepStrictEqual(result, expected);
  assert.strictEqual(checksum, n * (expected.length +
    (expected.length === 0 ? 0 : expected[expected.length - 1].length)));
}
