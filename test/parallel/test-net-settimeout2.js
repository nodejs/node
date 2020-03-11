'use strict';

const common = require('../common');
const net = require('net');
const assert = require('assert');
const { performance } = require('perf_hooks');

{
  // Updating timeout should not discard current stale time.
  // This can be important if using http keep-alive and we know
  // the server will kill connections after e.g. 2 min of inactivivity.
  // If the user changes the socket timeout we could lose elapsed
  // inativity time which increases the likelyhood of unexpected
  // ECONNRESET from the server.
  const server = net.createServer(common.mustCall((c) => {
    c.write('hello');
  }));

  server.listen(0, function() {
    const socket = net.createConnection(this.address().port, 'localhost');
    socket.resume();
    const start = performance.now();
    socket.setTimeout(1000);
    setTimeout(() => {
      socket.setTimeout(1000);
    }, 500);

    socket.on('timeout', common.mustCall(() => {
      const elapsed = performance.now() - start;
      assert(elapsed < 1100);
      server.close();
      socket.destroy();
    }));
  });
}
