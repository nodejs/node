'use strict';

const common = require('../common');
const assert = require('node:assert');
const querystring = require('node:querystring');

const bench = common.createBenchmark(main, {
  n: [1e5],
  api: ['querystring', 'URLSearchParams', 'URLSearchParams-new-instance'],
  type: ['ascii', 'latin1', 'mixed', 'twobyte', 'bmp', 'astral'],
  size: [16, 256],
});

function main({ n, api, type, size }) {
  const units = {
    ascii: 'abc/? &=',
    latin1: '\u00e9',
    mixed: 'abcd\u00e9',
    twobyte: '\u03b1',
    bmp: '\u4e2d',
    astral: '\ud83d\ude00',
  };
  const unit = units[type];
  const input = unit.repeat(Math.ceil(size / unit.length)).slice(0, size);
  const params = new URLSearchParams([['value', input]]);
  const expected = api === 'querystring' ? encodeURIComponent(input) :
    `value=${encodeURIComponent(input).replaceAll('%20', '+')}`;
  let actual;
  let checksum = 0;
  bench.start();
  if (api === 'querystring') {
    for (let index = 0; index < n; index++) {
      actual = querystring.escape(input);
      checksum += actual.charCodeAt(actual.length - 1);
    }
  } else if (api === 'URLSearchParams') {
    for (let index = 0; index < n; index++) {
      actual = params.toString();
      checksum += actual.charCodeAt(actual.length - 1);
    }
  } else {
    for (let index = 0; index < n; index++) {
      actual = new URLSearchParams({ value: input }).toString();
      checksum += actual.charCodeAt(actual.length - 1);
    }
  }
  bench.end(n);
  assert.strictEqual(actual, expected);
  assert.strictEqual(checksum, n * expected.charCodeAt(expected.length - 1));
}
