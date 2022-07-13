'use strict';

const common = require('../common');
const cluster = require('cluster');
const net = require('net');

if (cluster.isPrimary) {
  cluster.fork().on('message', function(msg) {
    if (msg === 'done') this.kill();
  });
} else {
  const server = net.createServer(common.mustNotCall());
  server.listen(0, function() {
    server.unref();
    server.ref();
    server.close(function() {
      process.send('done');
    });
  });
}
