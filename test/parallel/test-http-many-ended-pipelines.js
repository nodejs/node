'use strict';
var common = require('../common');
var assert = require('assert');

// no warnings should happen!
var trace = console.trace;
console.trace = function() {
  trace.apply(console, arguments);
  throw new Error('no tracing should happen here');
};

var http = require('http');
var net = require('net');

var numRequests = 20;
var done = 0;

var server = http.createServer(function(req, res) {
  res.end('ok');

  // Oh no!  The connection died!
  req.socket.destroy();
  if (++done == numRequests)
    server.close();
});

server.listen(common.PORT);

var client = net.connect({ port: common.PORT, allowHalfOpen: true });
for (var i = 0; i < numRequests; i++) {
  client.write('GET / HTTP/1.1\r\n' +
               'Host: some.host.name\r\n' +
               '\r\n\r\n');
}
client.end();
client.pipe(process.stdout);
