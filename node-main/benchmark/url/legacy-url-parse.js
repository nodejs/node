'use strict';
const common = require('../common.js');
const url = require('url');
const assert = require('assert');

const bench = common.createBenchmark(main, {
  type: common.urlDataTypes,
  e: [1],
});

function main({ e, type }) {
  const data = common.bakeUrlData(type, e, false, false);
  let result = url.parse(data[0]);  // Avoid dead code elimination

  bench.start();
  for (let i = 0; i < data.length; ++i) {
    result = url.parse(data[i]);
  }
  bench.end(data.length);

  assert.ok(result);
}
