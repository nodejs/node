'use strict';
require('../common');

// no warnings should happen!
var trace = console.trace;
console.trace = function() {
  trace.apply(console, arguments);
  throw new Error('no tracing should happen here');
};

var http = require('http');
var net = require('net');

var numRequests = 20;
var first = false;

var server = http.createServer(function(req, res) {
  if (!first) {
    first = true;
    req.socket.on('close', function() {
      server.close();
    });
  }

  res.end('ok');
  // Oh no!  The connection died!
  req.socket.destroy();
});

server.listen(0, function() {
  var client = net.connect({ port: this.address().port, allowHalfOpen: true });
  for (var i = 0; i < numRequests; i++) {
    client.write('GET / HTTP/1.1\r\n' +
                 'Host: some.host.name\r\n' +
                 '\r\n\r\n');
  }
  client.end();
  client.pipe(process.stdout);
});
