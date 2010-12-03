// This tests setTimeout() by having multiple clients connecting and sending
// data in random intervals. Clients are also randomly disconnecting until there
// are no more clients left. If no false timeout occurs, this test has passed.
var common = require('../common'),
    assert = require('assert'),
    http = require('http'),
    server = http.createServer(),
    connections = 0;

server.on('request', function(req, res) {
  req.socket.setTimeout(1000);
  req.socket.on('timeout', function() {
    throw new Error('Unexpected timeout');
  });
  req.on('end', function() {
    connections--;
    req.socket.end();
    if (connections == 0) {
      server.close();
    }
  });
});
server.listen(common.PORT, '127.0.0.1', function() {
  for (var i = 0; i < 10; i++) {
    connections++;

    setTimeout(function() {
      var client = http.createClient(common.PORT, '127.0.0.1'),
          request = client.request('POST', '/');

      function ping() {
        var nextPing = (Math.random() * 900).toFixed();
        if (nextPing > 600) {
          request.end();
          return;
        }
        request.write('ping');
        setTimeout(ping, nextPing);
      }
      ping();
    }, i * 50);
  }
});
