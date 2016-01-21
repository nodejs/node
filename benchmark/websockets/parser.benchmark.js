/*!
 * ws: a node.js websocket client
 * Copyright(c) 2011 Einar Otto Stangvik <einaros@gmail.com>
 * MIT Licensed
 */

/**
 * Benchmark dependencies.
 */

var benchmark = require('benchmark')
  , Receiver = require('../').Receiver
  , suite = new benchmark.Suite('Receiver');
require('tinycolor');
require('./util');

/**
 * Setup receiver.
 */
 
suite.on('start', function () {
  receiver = new Receiver();
});

suite.on('cycle', function () {
  receiver = new Receiver();
});

/**
 * Benchmarks.
 */

var pingMessage = 'Hello'
  , pingPacket1 = getBufferFromHexString('89 ' + (pack(2, 0x80 | pingMessage.length)) + 
                                         ' 34 83 a8 68 '+ getHexStringFromBuffer(mask(pingMessage, '34 83 a8 68')));
suite.add('ping message', function () {
  receiver.add(pingPacket1);  
});

var pingPacket2 = getBufferFromHexString('89 00')
suite.add('ping with no data', function () {
  receiver.add(pingPacket2);
});

var closePacket = getBufferFromHexString('88 00');
suite.add('close message', function () {
  receiver.add(closePacket);
  receiver.endPacket();
});

var maskedTextPacket = getBufferFromHexString('81 93 34 83 a8 68 01 b9 92 52 4f a1 c6 09 59 e6 8a 52 16 e6 cb 00 5b a1 d5');
suite.add('masked text message', function () {
  receiver.add(maskedTextPacket);
});

binaryDataPacket = (function() {
  var length = 125
    , message = new Buffer(length)
  for (var i = 0; i < length; ++i) message[i] = i % 10;
  return getBufferFromHexString('82 ' + getHybiLengthAsHexString(length, true) + ' 34 83 a8 68 '
       + getHexStringFromBuffer(mask(message), '34 83 a8 68'));
})();
suite.add('binary data (125 bytes)', function () {
  try {
    receiver.add(binaryDataPacket);
    
  }
  catch(e) {console.log(e)}
});

binaryDataPacket2 = (function() {
  var length = 65535
    , message = new Buffer(length)
  for (var i = 0; i < length; ++i) message[i] = i % 10;
  return getBufferFromHexString('82 ' + getHybiLengthAsHexString(length, true) + ' 34 83 a8 68 '
       + getHexStringFromBuffer(mask(message), '34 83 a8 68'));
})();
suite.add('binary data (65535 bytes)', function () {
  receiver.add(binaryDataPacket2);
});

binaryDataPacket3 = (function() {
  var length = 200*1024
    , message = new Buffer(length)
  for (var i = 0; i < length; ++i) message[i] = i % 10;
  return getBufferFromHexString('82 ' + getHybiLengthAsHexString(length, true) + ' 34 83 a8 68 '
       + getHexStringFromBuffer(mask(message), '34 83 a8 68'));
})();
suite.add('binary data (200 kB)', function () {
  receiver.add(binaryDataPacket3);
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
