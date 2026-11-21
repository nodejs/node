'use strict';

const common = require('../common');
const assert = require('node:assert');
const { EventEmitter } = require('node:events');

const bench = common.createBenchmark(main, {
  n: [1e6],
  size: [0, 4, 32],
  shape: ['strings', 'mixed', 'removed'],
  mode: ['read', 'churn'],
});

function main({ n, size, shape, mode }) {
  const emitter = new EventEmitter();
  const listener = () => {};
  const keys = [];
  for (let index = 0; index < size; index++) {
    const key = shape === 'mixed' && index % 2 === 0 ?
      Symbol(`event${index}`) : `event${index}`;
    emitter.on(key, listener);
    if (shape === 'removed' && index % 2 === 0) {
      emitter.removeListener(key, listener);
    } else {
      keys.push(key);
    }
  }
  if (mode === 'churn') keys.push('transient');
  const expected = [
    ...keys.filter((key) => typeof key === 'string'),
    ...keys.filter((key) => typeof key === 'symbol'),
  ];
  const last = expected[expected.length - 1];
  let checksum = 0;
  let result;
  bench.start();
  if (mode === 'read') {
    for (let iteration = 0; iteration < n; iteration++) {
      result = emitter.eventNames();
      checksum += result.length;
      if (result.length !== 0 && result[result.length - 1] === last) checksum++;
    }
  } else {
    for (let iteration = 0; iteration < n; iteration++) {
      emitter.on('transient', listener);
      result = emitter.eventNames();
      checksum += result.length;
      if (result.length !== 0 && result[result.length - 1] === last) checksum++;
      emitter.removeListener('transient', listener);
    }
  }
  bench.end(n);
  assert.deepStrictEqual(result, expected);
  assert.strictEqual(checksum, n * (expected.length + (expected.length !== 0)));
}
