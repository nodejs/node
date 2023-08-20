'use strict';
const common = require('../common');
const Countdown = require('../common/countdown');
const cluster = require('cluster');
const net = require('net');

// Test an edge case when using `cluster` and `net.Server.listen()` to
// the port of `0`.
const kPort = 0;

function child() {
  const kTime = 2;
  const countdown = new Countdown(kTime * 2, () => {
    process.exit(0);
  });
  for (let i = 0; i < kTime; i += 1) {
    const server = net.createServer();
    server.listen(kPort, common.mustCall(() => {
      server.close(countdown.dec());
      const server2 = net.createServer();
      server2.listen(kPort, common.mustCall(() => {
        server2.close(countdown.dec());
      }));
    }));
  }
}

if (cluster.isMaster)
  cluster.fork(__filename);
else
  child();
