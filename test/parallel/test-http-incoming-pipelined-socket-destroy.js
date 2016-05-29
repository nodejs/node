'use strict';
require('../common');

var http = require('http');
var net = require('net');


// Set up some timing issues where sockets can be destroyed
// via either the req or res.
var server = http.createServer(function(req, res) {
  switch (req.url) {
    case '/1':
      return setTimeout(function() {
        req.socket.destroy();
        server.emit('requestDone');
      });

    case '/2':
      return process.nextTick(function() {
        res.destroy();
        server.emit('requestDone');
      });

    // in one case, actually send a response in 2 chunks
    case '/3':
      res.write('hello ');
      return setTimeout(function() {
        res.end('world!');
        server.emit('requestDone');
      });

    default:
      res.destroy();
      server.emit('requestDone');
  }
});


// Make a bunch of requests pipelined on the same socket
function generator(seeds) {
  return seeds.map(function(r) {
    return 'GET /' + r + ' HTTP/1.1\r\n' +
           `Host: localhost:${server.address().port}\r\n` +
           '\r\n' +
           '\r\n';
  }).join('');
}


server.listen(0, function() {
  var seeds = [ 3, 1, 2, 3, 4, 1, 2, 3, 4, 1, 2, 3, 4 ];
  var client = net.connect({ port: this.address().port });
  var done = 0;
  server.on('requestDone', function() {
    if (++done == seeds.length) {
      server.close();
    }
  });

  // immediately write the pipelined requests.
  // Some of these will not have a socket to destroy!
  client.write(generator(seeds));
});

process.on('exit', function(c) {
  if (!c)
    console.log('ok');
});
