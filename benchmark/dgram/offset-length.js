// test UDP send/recv throughput with the "old" offset/length API
'use strict';

const common = require('../common.js');
const PORT = common.PORT;

// `num` is the number of send requests to queue up each time.
// Keep it reasonably high (>10) otherwise you're benchmarking the speed of
// event loop cycles more than anything else.
var bench = common.createBenchmark(main, {
  len: [1, 64, 256, 1024],
  num: [100],
  type: ['send', 'recv'],
  dur: [5]
});

var dur;
var len;
var num;
var type;
var chunk;

function main(conf) {
  dur = +conf.dur;
  len = +conf.len;
  num = +conf.num;
  type = conf.type;
  chunk = Buffer.allocUnsafe(len);
  server();
}

var dgram = require('dgram');

function server() {
  var sent = 0;
  var received = 0;
  var socket = dgram.createSocket('udp4');

  function onsend() {
    if (sent++ % num === 0)
      for (var i = 0; i < num; i++)
        socket.send(chunk, 0, chunk.length, PORT, '127.0.0.1', onsend);
  }

  socket.on('listening', function() {
    bench.start();
    onsend();

    setTimeout(function() {
      var bytes = (type === 'send' ? sent : received) * chunk.length;
      var gbits = (bytes * 8) / (1024 * 1024 * 1024);
      bench.end(gbits);
      process.exit(0);
    }, dur * 1000);
  });

  socket.on('message', function(buf, rinfo) {
    received++;
  });

  socket.bind(PORT);
}
