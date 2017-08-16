'use strict';
var common = require('../common.js');
var events = require('events');

var bench = common.createBenchmark(main, { n: [25e4] });

function main(conf) {
  var n = conf.n | 0;

  var ee = new events.EventEmitter();
  var listeners = [];

  var k;
  for (k = 0; k < 10; k += 1)
    listeners.push(function() {});

  bench.start();
  for (var i = 0; i < n; i += 1) {
    var dummy = (i % 2 === 0) ? 'dummy0' : 'dummy1';
    for (k = listeners.length; --k >= 0; /* empty */) {
      ee.on(dummy, listeners[k]);
    }
    for (k = listeners.length; --k >= 0; /* empty */) {
      ee.removeListener(dummy, listeners[k]);
    }
  }
  bench.end(n);
}
