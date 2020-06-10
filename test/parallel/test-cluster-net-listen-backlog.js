'use strict';

const common = require('../common');
const Countdown = require('../common/countdown');
const assert = require('assert');
// Monkey-patch `net.Server._listen2`
const net = require('net');
const cluster = require('cluster');

// Ensures that the `backlog` is used to create a `net.Server`.
const kExpectedBacklog = 127;
if (cluster.isMaster) {
  let worker

  const nativeListen = net.Server.prototype._listen2;
  const countdown = new Countdown(2, () => {
    worker.disconnect();
  });

  net.Server.prototype._listen2 = common.mustCall(
    function(address, port, addressType, backlog, fd, flags) {
      assert(backlog, kExpectedBacklog);
      countdown.dec();
      return nativeListen.call(this, address, port, addressType, backlog, fd, flags);
    }
  );

  worker = cluster.fork();
  worker.on('message', () => {
    countdown.dec();
  });
} else {
  const server = net.createServer()
  
  server.listen({
    host: common.localhostIPv4,
    port: 0,
    backlog: kExpectedBacklog,
  }, common.mustCall(() => {
    process.send(true);
  }));
}
