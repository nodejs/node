'use strict';
const common = require('../common.js');
const EventEmitter = require('events').EventEmitter;

const bench = common.createBenchmark(main, {
  n: [2e7],
  listeners: [1, 5, 10],
});

function main(conf) {
  var n = conf.n | 0;
  const listeners = Math.max(conf.listeners | 0, 1);

  const ee = new EventEmitter();

  if (listeners === 1)
    n *= 5;
  else if (listeners === 5)
    n *= 2;

  for (var k = 0; k < listeners; k += 1) {
    ee.on('dummy', function() {});
    ee.on(`dummy${k}`, function() {});
  }

  bench.start();
  for (var i = 0; i < n; i += 1) {
    if (i % 3 === 0)
      ee.emit('dummy', true, 5);
    else if (i % 2 === 0)
      ee.emit('dummy', true, 5, 10, false);
    else
      ee.emit('dummy');
  }
  bench.end(n);
}
