'use strict';
const common = require('../common');
var net = require('net');

var server = net.createServer(common.mustCall(function(stream) {
  stream.setTimeout(100);

  stream.resume();

  stream.on('timeout', common.mustCall(function() {
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
  var c = net.createConnection(this.address().port);

  c.on('data', function() {
    c.end();
  });

  c.on('end', function() {
    console.log('client side end');
    server.close();
  });
});
