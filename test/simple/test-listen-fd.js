// Verify that net.Server.listenFD() can be used to accept connections on an
// already-bound, already-listening socket.

require('../common');
http = require('http');
netBinding = process.binding('net');

// Select a random port for testing
var PORT = 5000 + Math.floor(Math.random() * 1000);

// Create an server and set it listening on a socket bound to PORT
var gotRequest = false;
var srv = http.createServer(function(req, resp) {
  gotRequest = true;

  resp.writeHead(200);
  resp.end();

  srv.close();
});

var fd = netBinding.socket('tcp4');
netBinding.bind(fd, PORT, '127.0.0.1');
netBinding.listen(fd, 128);
srv.listenFD(fd);

// Make an HTTP request to the server above
var hc = http.createClient(PORT, '127.0.0.1');
hc.request('/').end();

// Verify that we're exiting after having received an HTTP  request
process.addListener('exit', function() {
  assert.ok(gotRequest);
});
