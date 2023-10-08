'use strict';

const common = require('../common');
const assert = require('assert');

const bench = common.createBenchmark(main, {
  n: [1e7],
  value: [
    'application/ecmascript; ',
    'text/html;charset=gbk',
    // eslint-disable-next-line max-len
    'text/html;0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789=x;charset=gbk',
  ],
}, {
  flags: ['--expose-internals'],
});

function main({ n, value }) {

  const parseTypeAndSubtype = require('internal/mime').parseTypeAndSubtype;
  // Warm up.
  const length = 1024;
  const array = [];
  let errCase = false;

  for (let i = 0; i < length; ++i) {
    try {
      array.push(parseTypeAndSubtype(value));
    } catch (e) {
      errCase = true;
      array.push(e);
    }
  }

  // console.log(`errCase: ${errCase}`);
  bench.start();
  for (let i = 0; i < n; ++i) {
    const index = i % length;
    try {
      array[index] = parseTypeAndSubtype(value);
    } catch (e) {
      array[index] = e;
    }
  }

  bench.end(n);

  // Verify the entries to prevent dead code elimination from making
  // the benchmark invalid.
  for (let i = 0; i < length; ++i) {
    assert.strictEqual(typeof array[i], errCase ? 'object' : 'object');
  }
}
