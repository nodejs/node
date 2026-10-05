'use strict';

const common = require('../common');
const assert = require('assert');
const { MIMEType } = require('util');

const bench = common.createBenchmark(main, {
  n: [1e5],
  value: [
    'application/json',
    'text/html; charset=utf-8',
    'multipart/form-data; boundary=----WebKitFormBoundary7MA4YWxkTrZu0gW',
    'text/plain; charset="utf-8"; foo="b\\"ar"; x=y',
    'not a mime type',
  ],
  operation: ['parse', 'essence', 'params.get'],
});

function main({ n, value, operation }) {
  const length = 1024;
  const array = [];
  let fn;
  switch (operation) {
    case 'parse':
      fn = () => MIMEType.parse(value);
      break;
    case 'essence':
      fn = () => MIMEType.parse(value)?.essence ?? null;
      break;
    case 'params.get':
      fn = () => MIMEType.parse(value)?.params.get('charset') ?? null;
      break;
    default:
      throw new Error(`Unsupported operation ${operation}`);
  }

  // Warm up.
  for (let i = 0; i < length; ++i) {
    array.push(fn());
  }

  bench.start();
  for (let i = 0; i < n; ++i) {
    array[i % length] = fn();
  }
  bench.end(n);

  // Verify the entries to prevent dead code elimination from making
  // the benchmark invalid.
  for (let i = 0; i < length; ++i) {
    assert.notStrictEqual(array[i], undefined);
  }
}
