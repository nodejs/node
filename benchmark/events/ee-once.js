'use strict';
const common = require('../common.js');
const EventEmitter = require('events').EventEmitter;

const bench = common.createBenchmark(main, {
  n: [2e7],
  argc: [0, 1, 4, 5]
});

function main({ n, argc }) {
  const ee = new EventEmitter();

  function listener() {}

  switch (argc) {
    case 0:
      bench.start();
      for (let i = 0; i < n; i += 1) {
        const dummy = (i % 2 === 0) ? 'dummy0' : 'dummy1';
        ee.once(dummy, listener);
        ee.emit(dummy);
      }
      bench.end(n);
      break;
    case 1:
      bench.start();
      for (let i = 0; i < n; i += 1) {
        const dummy = (i % 2 === 0) ? 'dummy0' : 'dummy1';
        ee.once(dummy, listener);
        ee.emit(dummy, n);
      }
      bench.end(n);
      break;
    case 4:
      bench.start();
      for (let i = 0; i < n; i += 1) {
        const dummy = (i % 2 === 0) ? 'dummy0' : 'dummy1';
        ee.once(dummy, listener);
        ee.emit(dummy, 'foo', argc, 'bar', false);
      }
      bench.end(n);
      break;
    case 5:
      bench.start();
      for (let i = 0; i < n; i += 1) {
        const dummy = (i % 2 === 0) ? 'dummy0' : 'dummy1';
        ee.once(dummy, listener);
        ee.emit(dummy, true, 7.5, 'foo', null, n);
      }
      bench.end(n);
      break;
    default:
      throw new Error('Unsupported argument count');
  }
}
