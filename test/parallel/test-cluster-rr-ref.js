'use strict';

const common = require('../common');
const cluster = require('cluster');
const net = require('net');

if (cluster.isMaster) {
  cluster.fork().on('message', function(msg) {
    if (msg === 'done') this.kill();
  });
} else {
  const server = net.createServer(common.fail);
  server.listen(common.PORT, function() {
    server.unref();
    server.ref();
    server.close(function() {
      process.send('done');
    });
  });
}
