'use strict';
const common = require('../common.js');
const url = require('url');
const URL = url.URL;
const assert = require('assert');

const bench = common.createBenchmark(main, {
  type: common.urlDataTypes,
  method: ['legacy', 'whatwg'],
  e: [1]
});

function useLegacy(data) {
  const obj = url.parse(data[0]);
  const len = data.length;
  var noDead = url.format(obj);
  bench.start();
  for (var i = 0; i < len; i++) {
    noDead = data[i].toString();
  }
  bench.end(len);
  return noDead;
}

function useWHATWG(data) {
  const obj = new URL(data[0]);
  const len = data.length;
  var noDead = obj.toString();
  bench.start();
  for (var i = 0; i < len; i++) {
    noDead = data[i].toString();
  }
  bench.end(len);
  return noDead;
}

function main({ type, e, method }) {
  e = +e;
  const data = common.bakeUrlData(type, e, false, false);

  var noDead;  // Avoid dead code elimination.
  switch (method) {
    case 'legacy':
      noDead = useLegacy(data);
      break;
    case 'whatwg':
      noDead = useWHATWG(data);
      break;
    default:
      throw new Error(`Unknown method ${method}`);
  }

  assert.ok(noDead);
}
