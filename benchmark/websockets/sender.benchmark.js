/*!
 * ws: a node.js websocket client
 * Copyright(c) 2011 Einar Otto Stangvik <einaros@gmail.com>
 * MIT Licensed
 */

/**
 * Benchmark dependencies.
 */

var benchmark = require('benchmark')
  , Sender = require('../').Sender
  , suite = new benchmark.Suite('Sender');
require('tinycolor');
require('./util');

/**
 * Setup sender.
 */
 
suite.on('start', function () {
  sender = new Sender();
  sender._socket = { write: function() {} };
});

suite.on('cycle', function () {
  sender = new Sender();
  sender._socket = { write: function() {} };
});

/**
 * Benchmarks
 */

framePacket = new Buffer(200*1024);
framePacket.fill(99);
suite.add('frameAndSend, unmasked (200 kB)', function () {
  sender.frameAndSend(0x2, framePacket, true, false);
});
suite.add('frameAndSend, masked (200 kB)', function () {
  sender.frameAndSend(0x2, framePacket, true, true);
});

/**
 * Output progress.
 */

suite.on('cycle', function (bench, details) {
  console.log('\n  ' + suite.name.grey, details.name.white.bold);
  console.log('  ' + [
      details.hz.toFixed(2).cyan + ' ops/sec'.grey
    , details.count.toString().white + ' times executed'.grey
    , 'benchmark took '.grey + details.times.elapsed.toString().white + ' sec.'.grey
    , 
  ].join(', '.grey));
});

/**
 * Run/export benchmarks.
 */

if (!module.parent) {
  suite.run();
} else {
  module.exports = suite;
}
