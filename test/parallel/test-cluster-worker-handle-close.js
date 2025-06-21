'use strict';
const common = require('../common');
const cluster = require('cluster');
const net = require('net');

if (cluster.isPrimary) {
  cluster.schedulingPolicy = cluster.SCHED_RR;
  cluster.fork();
} else {
  const server = net.createServer(common.mustNotCall());
  server.listen(0, common.mustCall(() => {
    net.connect(server.address().port);
  }));
  process.prependListener('internalMessage', common.mustCallAtLeast((message, handle) => {
    if (message.act !== 'newconn') {
      return;
    }
    // Make the worker drops the connection, see `rr` and `onconnection` in child.js
    server.close();
    const close = handle.close;
    handle.close = common.mustCall(() => {
      close.call(handle, common.mustCall(() => {
        process.exit();
      }));
    });
  }));
}
