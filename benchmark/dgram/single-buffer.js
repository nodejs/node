// test UDP send/recv throughput with the new single Buffer API
'use strict';

const common = require('../common.js');
const dgram = require('dgram');
const PORT = common.PORT;

// `num` is the number of send requests to queue up each time.
// Keep it reasonably high (>10) otherwise you're benchmarking the speed of
// event loop cycles more than anything else.
const bench = common.createBenchmark(main, {
  len: [1, 64, 256, 1024],
  num: [100],
  type: ['send', 'recv'],
  dur: [5]
});

function main({ dur, len, num, type }) {
  const chunk = Buffer.allocUnsafe(len);
  var sent = 0;
  var received = 0;
  const socket = dgram.createSocket('udp4');

  function onsend() {
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
      const bytes = (type === 'send' ? sent : received) * chunk.length;
      const gbits = (bytes * 8) / (1024 * 1024 * 1024);
      bench.end(gbits);
      process.exit(0);
    }, dur * 1000);
  });

  socket.on('message', function() {
    received++;
  });

  socket.bind(PORT);
}
