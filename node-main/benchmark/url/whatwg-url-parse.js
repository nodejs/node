'use strict';
const common = require('../common.js');
const url = require('url');
const URL = url.URL;
const assert = require('assert');

const bench = common.createBenchmark(main, {
  withBase: ['true', 'false'],
  type: common.urlDataTypes,
  e: [1],
});

function useWHATWGWithBase(data) {
  const len = data.length;
  let result = new URL(data[0][0], data[0][1]);  // Avoid dead code elimination
  bench.start();
  for (let i = 0; i < len; ++i) {
    const item = data[i];
    result = new URL(item[0], item[1]);
  }
  bench.end(len);
  return result;
}

function useWHATWGWithoutBase(data) {
  const len = data.length;
  let result = new URL(data[0]);  // Avoid dead code elimination
  bench.start();
  for (let i = 0; i < len; ++i) {
    result = new URL(data[i]);
  }
  bench.end(len);
  return result;
}

function main({ e, type, withBase }) {
  withBase = withBase === 'true';
  const data = common.bakeUrlData(type, e, withBase, false);
  const noDead = withBase ? useWHATWGWithBase(data) : useWHATWGWithoutBase(data);

  assert.ok(noDead);
}
