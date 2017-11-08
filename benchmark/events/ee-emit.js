'use strict';
const common = require('../common.js');
const EventEmitter = require('events').EventEmitter;

const bench = common.createBenchmark(main, {
  n: [2e6],
  argc: [0, 2, 4, 10],
  listeners: [1, 5, 10],
});

function main(conf) {
  const n = conf.n | 0;
  const argc = conf.argc | 0;
  const listeners = conf.listeners | 0;

  const ee = new EventEmitter();

  const args = new Array(argc);
  args.fill(10);
  args.unshift('dummy');

  for (var k = 0; k < listeners; k += 1)
    ee.on('dummy', function() {});

  bench.start();
  for (var i = 0; i < n; i += 1) {
    ee.emit.apply(ee, args);
  }
  bench.end(n);
}
