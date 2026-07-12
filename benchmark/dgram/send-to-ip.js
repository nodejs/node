// Measure the send rate to a literal IP destination. The destination needs no
// name resolution, so this isolates the per-send overhead the default lookup
// pays before the packet reaches the socket.
'use strict';

const common = require('../common.js');
const dgram = require('dgram');
const PORT = common.PORT;

// `n` is the number of send requests queued each round. Keep it high (>10) so
// the measurement reflects send overhead rather than event loop cycles.
const bench = common.createBenchmark(main, {
  n: [100],
  dur: [5],
});

function main({ dur, n }) {
  const chunk = Buffer.allocUnsafe(1);
  let sent = 0;
  const socket = dgram.createSocket('udp4');

  function onsend() {
    if (sent++ % n === 0) {
      setImmediate(() => {
        for (let i = 0; i < n; i++) {
          socket.send(chunk, PORT, '127.0.0.1', onsend);
        }
      });
    }
  }

  socket.on('listening', () => {
    bench.start();
    onsend();

    setTimeout(() => {
      bench.end(sent);
      process.exit(0);
    }, dur * 1000);
  });

  socket.bind(PORT);
}
