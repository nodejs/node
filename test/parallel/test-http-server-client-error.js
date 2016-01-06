'use strict';
const common = require('../common');
const assert = require('assert');

const http = require('http');
const net = require('net');

const server = http.createServer(common.mustCall(function(req, res) {
  res.end();
}));

server.on('clientError', common.mustCall(function(err, socket) {
  socket.end('HTTP/1.1 400 Bad Request\r\n\r\n');

  server.close();
}));

server.listen(common.PORT, function() {
  function next() {
    // Invalid request
    const client = net.connect(common.PORT);
    client.end('Oopsie-doopsie\r\n');

    var chunks = '';
    client.on('data', function(chunk) {
      chunks += chunk;
    });
    client.once('end', function() {
      assert.equal(chunks, 'HTTP/1.1 400 Bad Request\r\n\r\n');
    });
  }

  // Normal request
  http.get({ port: common.PORT, path: '/' }, function(res) {
    assert.equal(res.statusCode, 200);
    res.resume();
    res.once('end', next);
  });
});
