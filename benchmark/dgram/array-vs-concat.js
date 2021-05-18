// Test UDP send throughput with the multi buffer API against Buffer.concat
'use strict';

const common = require('../common.js');
const dgram = require('dgram');
const PORT = common.PORT;

// `num` is the number of send requests to queue up each time.
// Keep it reasonably high (>10) otherwise you're benchmarking the speed of
// event loop cycles more than anything else.
const bench = common.createBenchmark(main, {
  len: [64, 256, 512, 1024],
  num: [100],
  chunks: [1, 2, 4, 8],
  type: ['concat', 'multi'],
  dur: [5],
});

function main({ dur, len, num, type, chunks }) {
  const chunk = [];
  for (let i = 0; i < chunks; i++) {
    chunk.push(Buffer.allocUnsafe(Math.round(len / chunks)));
  }

  // Server
  let sent = 0;
  const socket = dgram.createSocket('udp4');
  const onsend = type === 'concat' ? onsendConcat : onsendMulti;

  function onsendConcat() {
    if (sent++ % num === 0) {
      // The setImmediate() is necessary to have event loop progress on OSes
      // that only perform synchronous I/O on nonblocking UDP sockets.
      setImmediate(() => {
        for (let i = 0; i < num; i++) {
          socket.send(Buffer.concat(chunk), PORT, '127.0.0.1', onsend);
        }
      });
    }
  }

  function onsendMulti() {
    if (sent++ % num === 0) {
      // The setImmediate() is necessary to have event loop progress on OSes
      // that only perform synchronous I/O on nonblocking UDP sockets.
      setImmediate(() => {
        for (let i = 0; i < num; i++) {
          socket.send(chunk, PORT, '127.0.0.1', onsend);
        }
      });
    }
  }

  socket.on('listening', () => {
    bench.start();
    onsend();

    setTimeout(() => {
      const bytes = sent * len;
      const gbits = (bytes * 8) / (1024 * 1024 * 1024);
      bench.end(gbits);
      process.exit(0);
    }, dur * 1000);
  });

  socket.bind(PORT);
}
