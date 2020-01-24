'use strict';
const common = require('../common.js');
const EventEmitter = require('events').EventEmitter;

const bench = common.createBenchmark(main, {
  n: [2e6],
  argc: [0, 2, 4, 10],
  listeners: [1, 5, 10],
});

function main({ n, argc, listeners }) {
  const ee = new EventEmitter();

  for (let k = 0; k < listeners; k += 1)
    ee.on('dummy', () => {});

  switch (argc) {
    case 2:
      bench.start();
      for (let i = 0; i < n; i += 1) {
        ee.emit('dummy', true, 5);
      }
      bench.end(n);
      break;
    case 4:
      bench.start();
      for (let i = 0; i < n; i += 1) {
        ee.emit('dummy', true, 5, 10, false);
      }
      bench.end(n);
      break;
    case 10:
      bench.start();
      for (let i = 0; i < n; i += 1) {
        ee.emit('dummy', true, 5, 10, false, 5, 'string', true, false, 11, 20);
      }
      bench.end(n);
      break;
    default:
      bench.start();
      for (let i = 0; i < n; i += 1) {
        ee.emit('dummy');
      }
      bench.end(n);
      break;
  }
}
