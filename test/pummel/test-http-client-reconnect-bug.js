'use strict';
const common = require('../common');
const net = require('net');
const http = require('http');

var server = net.createServer(function(socket) {
  socket.end();
});

server.on('listening', common.mustCall(function() {
  var client = http.createClient(common.PORT);

  client.on('error', common.mustCall(function(err) {}));
  client.on('end', common.mustCall(function() {}));

  var request = client.request('GET', '/', {'host': 'localhost'});
  request.end();
  request.on('response', function(response) {
    console.log('STATUS: ' + response.statusCode);
  });
}));

server.listen(common.PORT);

setTimeout(function() {
  server.close();
}, 500);
