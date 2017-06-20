// test UDP send throughput with the multi buffer API against Buffer.concat
'use strict';

const common = require('../common.js');
const PORT = common.PORT;

// `num` is the number of send requests to queue up each time.
// Keep it reasonably high (>10) otherwise you're benchmarking the speed of
// event loop cycles more than anything else.
var bench = common.createBenchmark(main, {
  len: [64, 256, 512, 1024],
  num: [100],
  chunks: [1, 2, 4, 8],
  type: ['concat', 'multi'],
  dur: [5]
});

var dur;
var len;
var num;
var type;
var chunk;
var chunks;

function main(conf) {
  dur = +conf.dur;
  len = +conf.len;
  num = +conf.num;
  type = conf.type;
  chunks = +conf.chunks;

  chunk = [];
  for (var i = 0; i < chunks; i++) {
    chunk.push(Buffer.allocUnsafe(Math.round(len / chunks)));
  }

  server();
}

var dgram = require('dgram');

function server() {
  var sent = 0;
  var socket = dgram.createSocket('udp4');

  var onsend = type === 'concat' ? onsendConcat : onsendMulti;

  function onsendConcat() {
    if (sent++ % num === 0) {
      for (var i = 0; i < num; i++) {
        socket.send(Buffer.concat(chunk), PORT, '127.0.0.1', onsend);
      }
    }
  }

  function onsendMulti() {
    if (sent++ % num === 0) {
      for (var i = 0; i < num; i++) {
        socket.send(chunk, PORT, '127.0.0.1', onsend);
      }
    }
  }

  socket.on('listening', function() {
    bench.start();
    onsend();

    setTimeout(function() {
      var bytes = sent * len;
      var gbits = (bytes * 8) / (1024 * 1024 * 1024);
      bench.end(gbits);
      process.exit(0);
    }, dur * 1000);
  });

  socket.bind(PORT);
}
