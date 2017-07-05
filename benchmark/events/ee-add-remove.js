'use strict';
var common = require('../common.js');
var events = require('events');

var bench = common.createBenchmark(main, {n: [25e4], listeners: [10]});

function main(conf) {
  var n = conf.n | 0;
  var listeners = conf.listeners | 0;

  var ee = new events.EventEmitter();
  ee.setMaxListeners(listeners + 1);
  var fns = [];

  var k;
  for (k = 0; k < listeners; k += 1)
    fns.push(function() { return 0; });

  bench.start();
  for (var i = 0; i < n; i += 1) {
    for (k = listeners; --k >= 0; /* empty */)
      ee.on('dummy', fns[k]);
    for (k = listeners; --k >= 0; /* empty */)
      ee.removeListener('dummy', fns[k]);
  }
  bench.end(n);
}
