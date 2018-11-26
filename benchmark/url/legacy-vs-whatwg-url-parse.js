'use strict';
const common = require('../common.js');
const url = require('url');
const URL = url.URL;
const assert = require('assert');

const bench = common.createBenchmark(main, {
  withBase: ['true', 'false'],
  type: common.urlDataTypes,
  e: [1],
  method: ['legacy', 'whatwg']
});

function useLegacy(data) {
  const len = data.length;
  var result = url.parse(data[0]);  // avoid dead code elimination
  bench.start();
  for (var i = 0; i < len; ++i) {
    result = url.parse(data[i]);
  }
  bench.end(len);
  return result;
}

function useWHATWGWithBase(data) {
  const len = data.length;
  var result = new URL(data[0][0], data[0][1]);  // avoid dead code elimination
  bench.start();
  for (var i = 0; i < len; ++i) {
    const item = data[i];
    result = new URL(item[0], item[1]);
  }
  bench.end(len);
  return result;
}

function useWHATWGWithoutBase(data) {
  const len = data.length;
  var result = new URL(data[0]);  // avoid dead code elimination
  bench.start();
  for (var i = 0; i < len; ++i) {
    result = new URL(data[i]);
  }
  bench.end(len);
  return result;
}

function main({ e, method, type, withBase }) {
  e = +e;
  withBase = withBase === 'true';
  var noDead;  // Avoid dead code elimination.
  var data;
  switch (method) {
    case 'legacy':
      data = common.bakeUrlData(type, e, false, false);
      noDead = useLegacy(data);
      break;
    case 'whatwg':
      data = common.bakeUrlData(type, e, withBase, false);
      noDead = withBase ? useWHATWGWithBase(data) : useWHATWGWithoutBase(data);
      break;
    default:
      throw new Error(`Unknown method ${method}`);
  }

  assert.ok(noDead);
}
