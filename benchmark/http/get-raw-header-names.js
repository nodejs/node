'use strict';

const common = require('../common.js');
const assert = require('node:assert');
const { OutgoingMessage } = require('node:http');

const bench = common.createBenchmark(main, {
  n: [1e6],
  headers: [0, 1, 4, 16, 64],
});

function main({ n, headers }) {
  const message = new OutgoingMessage();
  const expected = [];
  for (let index = 0; index < headers; index++) {
    const name = `X-Header-${index}`;
    message.setHeader(name, 'value');
    expected.push(name);
  }

  for (let index = 0; index < 1e4; index++) {
    message.getRawHeaderNames();
  }

  let result;
  bench.start();
  for (let index = 0; index < n; index++) {
    result = message.getRawHeaderNames();
  }
  bench.end(n);

  assert.deepStrictEqual(result, expected);
}
