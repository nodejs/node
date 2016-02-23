'use strict';
// This tests setTimeout() by having multiple clients connecting and sending
// data in random intervals. Clients are also randomly disconnecting until there
// are no more clients left. If no false timeout occurs, this test has passed.
const common = require('../common');
const http = require('http');
const server = http.createServer();
let connections = 0;

server.on('request', function(req, res) {
  req.socket.setTimeout(1000);
  req.socket.on('timeout', function() {
    throw new Error('Unexpected timeout');
  });
  req.on('end', function() {
    connections--;
    res.writeHead(200);
    res.end('done\n');
    if (connections == 0) {
      server.close();
    }
  });
  req.resume();
});

server.listen(common.PORT, '127.0.0.1', function() {
  for (var i = 0; i < 10; i++) {
    connections++;

    setTimeout(function() {
      var request = http.request({
        port: common.PORT,
        method: 'POST',
        path: '/'
      });

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
