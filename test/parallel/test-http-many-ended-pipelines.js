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

  var callbackCalled = false;

  res.end('ok', function() {
    assert.ok(!callbackCalled);
    callbackCalled = true;
  });

  // We might get a socket already closed error here
  // Not really a surpise
  res.on('error', function() {});

  res.on('close', function() {
    process.nextTick(function() {
      assert.ok(callbackCalled, "end() callback should've been called");
    });
  });

  // Oh no!  The connection died!
  req.socket.destroy();

  if (++done == numRequests)
    server.close();
});

server.listen(common.PORT);

var client = net.connect({ port: common.PORT, allowHalfOpen: true });
for (var i = 0; i < numRequests; i++) {
  client.write('GET / HTTP/1.1\r\n' +
               'Host: some.host.name\r\n'+
               '\r\n\r\n');
}

client.end();

client.pipe(process.stdout);
