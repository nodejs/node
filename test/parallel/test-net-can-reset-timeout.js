'use strict';
const common = require('../common');

// Ref: https://github.com/nodejs/node-v0.x-archive/issues/481

const net = require('net');

const server = net.createServer(common.mustCall(function(stream) {
  stream.setTimeout(100);

  stream.resume();

  stream.once('timeout', common.mustCall(function() {
    console.log('timeout');
    // try to reset the timeout.
    stream.write('WHAT.');
  }));

  stream.on('end', function() {
    console.log('server side end');
    stream.end();
  });
}));

server.listen(0, function() {
  const c = net.createConnection(this.address().port);

  c.on('data', function() {
    c.end();
  });

  c.on('end', function() {
    console.log('client side end');
    server.close();
  });
});
