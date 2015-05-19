'use strict';
if (process.platform === 'win32') {
  console.log('skipping test on windows, where clustered dgram is ENOTSUP');
  process.exit(0);
}

var cluster = require('cluster');
var dgram = require('dgram');

if (cluster.isMaster) {
  var unbound = cluster.fork().on('online', bind);

  function bind() {
    cluster.fork({BOUND: 'y'}).on('listening', disconnect);
  }

  function disconnect() {
    unbound.disconnect();
    unbound.on('disconnect', cluster.disconnect);
  }
} else {
  if (process.env.BOUND === 'y') {
    var source = dgram.createSocket('udp4');

    source.bind(0);
  }
}
