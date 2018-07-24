'use strict';
const common = require('../common.js');
const EventEmitter = require('events').EventEmitter;

const bench = common.createBenchmark(main, {
  n: [5e6],
  listenerCount: [1, 5, 10],
});


function main({ n, listenerCount }) {
  const ee = new EventEmitter();

  function listener() {}

  bench.start();
  for (var i = 0; i < n; ++i) {
    const dummy = (i % 2 === 0) ? 'dummy0' : 'dummy1';
    for (var j = 0; j < listenerCount; ++j)
      ee.once(dummy, listener);
    ee.emit(dummy);
  }
  bench.end(n);
}
