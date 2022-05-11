'use strict';
const common = require('../common.js');
const { AsyncResource } = require('async_hooks');

const noop = () => {};
const thisArg = {};
const resource = new AsyncResource('test');

const bench = common.createBenchmark(main, {
  n: [1e100],
  option: ['withThisArg', 'withoutThisArg']
});

function main({ n, option }) {
  bench.start();
  for (let i = 0; i < 1e7; i++) {
    resource.bind(noop, null, option === 'withThisArg' ? thisArg : undefined);
  }
  bench.end(n);
}
