// Flags: --expose-internals
'use strict';

const common = require('../common');
const internalUtil = require('internal/util');

const bench = common.createBenchmark(main, {
  method: ['Object.assign()', 'util._extend()'],
  size: [5, 10, 15, 30],
  n: [1e5]
});

function makeObject(size) {
  var m = {};
  for (var n = 0; n < size; n++)
    m[`key${n}`] = n;
  return m;
}

function useObjectAssign(n, obj) {
  bench.start();
  for (var i = 0; i < n; i++) {
    Object.assign({}, obj);
  }
  bench.end(n);
}

function useUtilExtend(n, obj) {
  bench.start();
  for (var i = 0; i < n; i++) {
    internalUtil._extend({}, obj);
  }
  bench.end(n);
}

function main(conf) {
  const n = +conf.n;
  const obj = makeObject(+conf.size);

  switch (conf.method) {
    case 'Object.assign()':
      useObjectAssign(n, obj);
      break;
    case 'util._extend()':
      useUtilExtend(n, obj);
      break;
    default:
      throw new Error('Unexpected method');
  }
}
